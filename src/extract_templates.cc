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

static llvm::cl::OptionCategory MatcherSampleCategory("Matcher Sample");

class CXXConstructExprHandler : public MatchFinder::MatchCallback {
public:
  CXXConstructExprHandler() {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    // The matched 'if' statement was bound to 'CXXConstructExpr'.
    if (const ClassTemplateSpecializationDecl *crd =
        Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("template_instantiation")) {
      std::cout << "Found a template instantiation" << std::endl;

      ClassTemplateDecl *ctd = crd->getSpecializedTemplate();
      std::cout << ctd->getQualifiedNameAsString() << std::endl;

      auto& template_args = crd->getTemplateArgs();
      for (size_t i = 0; i != template_args.size(); ++i) {
        auto& arg = template_args[i];
        auto type = arg.getAsType();
        std::cout << type.getAsString() << std::endl;
      }

      //crd->dumpColor();
      std::cout << std::endl;
    }
  }
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser. It registers a couple of matchers and runs them on
// the AST.
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer() :
    HandlerForIf()
  {
    // Add a simple matcher for finding 'if' statements.
    //Matcher.addMatcher(constructExpr().bind("template_instantiation"), &HandlerForIf);
    Matcher.addMatcher(classTemplateSpecializationDecl().bind("template_instantiation"), &HandlerForIf);
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    // Run the matchers when we have the whole TU parsed.
    Matcher.matchAST(Context);
  }

private:
  CXXConstructExprHandler HandlerForIf;
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
