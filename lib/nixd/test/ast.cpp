#include <exception>
#include <gtest/gtest.h>

#include "installable-value.hh"
#include "nixd/AST.h"
#include "nixd/Expr.h"
#include "nixutil.h"
#include "value.hh"

#include <nix/command-installable-value.hh>

namespace nixd {

TEST(AST, withEvaluation) {
  InitNix INix;
  constexpr int NumWorkers = 15;

  static const char *NixSrc = R"(
        let x = { a = 1; b = 2; }; in x
    )";

  class MyCmd : public nix::InstallableValueCommand {
    void run(nix::ref<nix::Store>, nix::ref<nix::InstallableValue>) override {}
  } Cmd;

  auto State = Cmd.getEvalState();
  auto *Root = State->parseExprFromString(NixSrc, "/");
  ASTContext Cxt;
  boost::asio::thread_pool Pool;
  NixAST AST(Cxt, Root);
  AST.injectAST(*State, "/");

  Cmd.parseCmdline({"--file", "/"});

  auto Installable =
      nix::InstallableValue::require(Cmd.parseInstallable(Cmd.getStore(), ""));

  AST.withEvaluation(Pool, Installable, std::move(State),
                     [](std::exception *Except, NixAST *AST) {
                       auto RootValue = AST->getValue(AST->root());
                       ASSERT_TRUE(RootValue.isTrivial());
                       ASSERT_EQ(RootValue.type(), nix::ValueType::nAttrs);
                       ASSERT_EQ(RootValue.attrs->capacity(), 2);
                     });
}

} // namespace nixd
