#include <exception>
#include <gtest/gtest.h>

#include "installable-value.hh"
#include "nixd/AST.h"
#include "nixd/Expr.h"
#include "nixutil.h"

#include <nix/command-installable-value.hh>

namespace nixd {

TEST(AST, withEvaluation) {
  InitNix INix;
  constexpr int NumWorkers = 15;

  static const char *NixSrc = R"(
        let x = 1; in x
    )";

  class MyCmd : public nix::InstallableValueCommand {
    void run(nix::ref<nix::Store>, nix::ref<nix::InstallableValue>) override {}
  } Cmd;

  auto State = Cmd.getEvalState();
  auto *Root = State->parseExprFromString(NixSrc, "/dev");
  ASTContext Cxt;
  Scheduler MyScheduler(NumWorkers);
  NixAST AST(Cxt, Root);
  AST.injectAST(*State, "/dev/null");

  Cmd.parseCmdline({"--file", "/dev/null"});

  auto Installable = nix::InstallableValue::require(
      Cmd.parseInstallable(Cmd.getStore(), "/dev/null"));

  AST.withEvaluation(MyScheduler, "", Installable, std::move(State),
                     [](std::exception *Except, NixAST *AST) {
                       auto *RootValue = AST->getValue(AST->root());
                       ASSERT_TRUE(RootValue->isTrivial());
                       ASSERT_TRUE(RootValue->integer == 1);
                     });
}

} // namespace nixd
