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
#include <iostream>
#include <string>

#include "libtooling_headers.hpp"  // <-- included in CMakeLists as a PCH

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

// Class declarations are CXXRecordDecl
// template class declarations are ClassTemplateDecl

static llvm::cl::OptionCategory MatcherSampleCategory("Matcher Sample");

class CXXConstructExprHandler : public MatchFinder::MatchCallback {
public:
  CXXConstructExprHandler() : context_(nullptr) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    // The matched statement was bound to 'CXXConstructExpr'.
    if (const CXXConstructExpr *match =
        Result.Nodes.getNodeAs<CXXConstructExpr>("construction")) {
      //match->dumpColor();
      auto &source_manager = context_->getSourceManager();
      auto location = match->getLocation();

      // Check that the constructor was called from the main source file (and
      // not, e.g. from something included or generated).
      if (source_manager.isInMainFile(location)) {
        QualType qual_type = match->getType();

        // Check that the constructor's origin can be traced back to a
        // template class.
        if (const CXXRecordDecl *match_declaration =
            qual_type
            .getTypePtr()
            ->getAsCXXRecordDecl()
            ->getTemplateInstantiationPattern()) {
          if (const ClassTemplateDecl *match_template_declaration =
              match_declaration
              ->getDescribedClassTemplate()) {
            std::cout << "Found a class instantiation at "
              << match->getExprLoc().printToString(context_->getSourceManager())
              << std::endl;
            std::cout << "QualType is " << qual_type.getAsString() << std::endl;
            std::cout << "CXXRecordDecl is "
              << match_declaration->getNameAsString()
              << std::endl;
            std::cout << "ClassTemplateDecl is "
              << match_template_declaration->getNameAsString()
              << " located at "
              << match_template_declaration
                 ->getLocation().printToString(context_->getSourceManager())
              << std::endl;
            auto param_list =
              match_template_declaration->getTemplateParameters();
            size_t num_params = param_list->size();
            for (size_t idx=0; idx != num_params; ++idx) {
              auto param = param_list->getParam(idx);
              std::cout << " - Parameter " << idx+1 << " : "
                << param->getNameAsString() << std::endl;
            }
            std::cout << std::endl;
          }
        }

      }

//      ClassTemplateDecl *ctd = match->getSpecializedTemplate();
//      std::cout << ctd->getQualifiedNameAsString() << std::endl;
//
//      auto& template_args = match->getTemplateArgs();
//      for (size_t i = 0; i != template_args.size(); ++i) {
//        auto& arg = template_args[i];
//        auto type = arg.getAsType();
//        std::cout << type.getAsString() << std::endl;
//      }

    }
  }

  void SetContext(ASTContext *context) { context_ = context; }

private:
  ASTContext *context_;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser. It registers a couple of matchers and runs them on
// the AST.
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer() :
    extract_template_handler_()
  {
    // Add a simple matcher for finding 'if' statements.
    //Matcher.addMatcher(constructExpr().bind("template_instantiation"),
    //                   &extract_template_handler_);
    Matcher.addMatcher(constructExpr().bind("construction"),
                       &extract_template_handler_);
  }

  void HandleTranslationUnit(ASTContext &context) override {
    // Run the matchers when we have the whole TU parsed.

    extract_template_handler_.SetContext(&context);

    Matcher.matchAST(context);

//    std::cout << "We have " << Context.getTypes().size() << " types" << std::endl;
//    auto &types = Context.getTypes();
//    size_t spec_count = 0;
//    for (const auto &type : types) {
//      if (const TemplateSpecializationType *temp_t =
//          type->getAs<TemplateSpecializationType>()) {
//        spec_count++;
//        if (!(spec_count % 300)) {
//          temp_t->dump();
//          std::cout << std::endl;
//        }
//      }
//    }
//    std::cout << "Of which " << spec_count << " are template specializations"
//      << std::endl;
  }

private:
  CXXConstructExprHandler extract_template_handler_;
  MatchFinder Matcher;
};

// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
public:
  MyFrontendAction() {}
  void EndSourceFileAction() override {
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                           StringRef file) override {
    return llvm::make_unique<MyASTConsumer>();
  }
};

int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, MatcherSampleCategory);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}
