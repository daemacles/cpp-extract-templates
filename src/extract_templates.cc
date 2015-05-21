//------------------------------------------------------------------------------
// AST matching sample. Demonstrates:
//
// * How to write a simple source tool using libTooling.
// * How to use AST matchers to find interesting AST nodes.
// * How to use the Rewriter API to rewrite the source code.
//
// Eli Bendersky (eliben@gmail.com)
// This code is in the public domain
//------------------------------------------------------------------------------

// Notes:
// Class declarations are CXXRecordDecl
// Template class declarations are ClassTemplateDecl

#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "libtooling_headers.hpp"  // <-- included in CMakeLists as a PCH

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory MatcherSampleCategory("Matcher Sample");

std::vector<std::string> split(const std::string &s, char delim) {
  std::stringstream ss(s);
  std::string item;
  std::vector<std::string> elems;
  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

struct TemplateInstantiationInfo {
  std::string type;
  std::string source;
};

typedef std::map<std::string, TemplateInstantiationInfo> InfoMap;

class CXXConstructExprHandler : public MatchFinder::MatchCallback {
public:
  CXXConstructExprHandler() {}

  virtual void run(const MatchFinder::MatchResult &result) {
    // The matched statement was bound to a CXXConstructExpr.
    if (const CXXConstructExpr *match =
        result.Nodes.getNodeAs<CXXConstructExpr>("construction"))
    {
      auto &source_manager = result.Context->getSourceManager();
      auto location = match->getLocation();

      // Check that the constructor was called from the main source file (and
      // not, e.g. from something included or generated).
      if (source_manager.isInMainFile(location)) {
        // qual_type is the untemplated class type.  We need to get the
        // CXXRecordDecl associated with the actual template declaration --
        // recall that a template class has both a CXXRecordDecl and a
        // ClassTemplateDecl.
        QualType qual_type = match->getType();

        // Check that the constructor's origin can be traced back to a
        // template class by retrieving the CXXRecordDecl actually associated
        // with the template declaration in source via the
        // getTemplateInstantiationPattern() method.  For some reason, the
        // CXXRecordDecl directly associated with the qual_type is not the
        // real CXXRecordDecl.
        if (const CXXRecordDecl *match_declaration =
            qual_type
            .getTypePtr()
            ->getAsCXXRecordDecl()  // upcast from Type*
            ->getTemplateInstantiationPattern())
        {
          // Finally, check that we can access the associated
          // ClassTemplateDecl.
          if (const ClassTemplateDecl *match_template_declaration =
              match_declaration->getDescribedClassTemplate())
          {
            // Skip template classes defined in the main file.
            if (source_manager
                .isInMainFile(match_template_declaration->getLocation()))
            {
              return;
            }

            // If we get here, must be a good match.
            TemplateInstantiationInfo info;
            info.type = qual_type
                        .getCanonicalType()    // includes template params
                        .getUnqualifiedType()  // removes e.g. const
                        .getAsString();
            info.source = source_manager.getFilename(match_template_declaration
                                                     ->getLocation()).str();
            info_map_[info.type] = info;
          }
        }
      }
    }
  }

  void printInfo (const CXXConstructExpr &match,
                  const SourceManager &source_manager,
                  const ClassTemplateDecl &match_template_declaration,
                  const TemplateInstantiationInfo &info) {
    // Print some info.
    std::cout
      << match.getExprLoc().printToString(source_manager)
      << ":" << std::endl;
    std::cout << "  " << info.type << std::endl;
    std::cout << "    "
      << match_template_declaration.getNameAsString()
      << " <";
    auto param_list =
      match_template_declaration.getTemplateParameters();
    int num_params = param_list->size();
    for (int idx=0; idx < num_params-1; ++idx) {
      auto param = param_list->getParam(idx);
      std::cout << param->getNameAsString() << ", ";
    }
    std::cout
      << param_list
      ->getParam(num_params-1)
      ->getNameAsString()
      << ">   @ "
      << info.source << std::endl;
    std::cout << std::endl;
  }

  const InfoMap& GetInfoMap () { return info_map_; }

private:
  InfoMap info_map_;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser. It registers a couple of matchers and runs them on
// the AST.
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer(CXXConstructExprHandler &handler) :
    extract_template_handler_(handler)
  {
    // Add a simple matcher for finding 'if' statements.
    //Matcher.addMatcher(constructExpr().bind("template_instantiation"),
    //                   &extract_template_handler_);
    Matcher.addMatcher(constructExpr().bind("construction"),
                       &extract_template_handler_);
  }

  void HandleTranslationUnit(ASTContext &context) override {
    // Run the matchers when we have the whole TU parsed.

    Matcher.matchAST(context);
  }

private:
  CXXConstructExprHandler &extract_template_handler_;
  MatchFinder Matcher;
};

// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
public:
  MyFrontendAction(CXXConstructExprHandler *handle) :
      extract_template_handler_(handle)
  {}

  void EndSourceFileAction() override {
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                           StringRef file) override {
    auto my_consumer =
      llvm::make_unique<MyASTConsumer>(*extract_template_handler_);
    std::unique_ptr<ASTConsumer> consumer(std::move(my_consumer));
    return consumer;
  }

private:
  CXXConstructExprHandler *extract_template_handler_;
};

// Define a custom FrontendActionFactory that can pass a parameter(s) to the
// created FrontendActions.
template<typename T, typename SettingsT>
class ParameterizedFrontendActionFactory : public FrontendActionFactory {
public:
  ParameterizedFrontendActionFactory (SettingsT settings) :
      FrontendActionFactory(),
      settings_(settings)
  {}

  FrontendAction* create() override {
    return new T(settings_);
  }

private:
  SettingsT settings_;
};


// Convert headers where the template looks like it's coming from, e.g., a
// system header since the declaration location has been included by the
// system header.  For example, std::vector maps directly to
// <bits/stl_vector.h>, but the user should really be including <vector>.
//
// Currently this is just done by matching the filename without any path
// information.
class HeaderFileTransformer {
public:
  HeaderFileTransformer () {
    standard_headers_[""] = "";
    standard_headers_["stringfwd.h"] = "string";
    standard_headers_["stl_set.h"] = "set";
    standard_headers_["random.h"] = "random";
    standard_headers_["stl_vector.h"] = "vector";
  }

  InfoMap Transform (const InfoMap &infos) {
    InfoMap transformed;
    for (const auto &type_info : infos) {
      const auto& info = type_info.second;
      transformed[info.type] = type_info.second;
      auto parts = split(info.source, '/');
      std::string filename = parts.back();
      if (standard_headers_.count(filename)) {
        transformed[info.type].source = standard_headers_.at(filename);
      }
    }

    return transformed;
  }

private:
  std::map<std::string, std::string> standard_headers_;
};


int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, MatcherSampleCategory);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  CXXConstructExprHandler extract_template_handler;

  std::unique_ptr<FrontendActionFactory> factory(
    new ParameterizedFrontendActionFactory<MyFrontendAction,
        CXXConstructExprHandler*>(&extract_template_handler));

  auto res = Tool.run(factory.get());

  HeaderFileTransformer transformer;
  InfoMap extracted_templates = transformer.Transform(extract_template_handler.GetInfoMap());

  std::set<std::string> headers;
  for (const auto &type_info : extracted_templates) {
    std::cout << "template " << type_info.first << std::endl;
    headers.insert(type_info.second.source);
  }
  std::cout << std::endl;
  for (const auto &header : headers) {
    std::cout << "#include \"" << header << "\"" << std::endl;
  }

  return res;
}
