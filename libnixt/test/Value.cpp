#include "StateTest.h"

#include "nixt/Value.h"

using namespace nixt;

namespace {

struct ValueTest : StateTest {
  nix::SourcePath cwd() { return State->rootPath(nix::CanonPath::fromCwd()); }
};

TEST_F(ValueTest, IsOption_neg) {
  nix::Expr *AST = State->parseExprFromString("1", cwd());
  nix::Value V;
  State->eval(AST, V);

  ASSERT_FALSE(isOption(*State, V));
}

TEST_F(ValueTest, IsOption_pos) {
  nix::Expr *AST =
      State->parseExprFromString(R"({ _type = "option"; })", cwd());
  nix::Value V;
  State->eval(AST, V);

  ASSERT_TRUE(isOption(*State, V));
}

TEST_F(ValueTest, IsDerivation_neg) {
  nix::Expr *AST =
      State->parseExprFromString(R"({ _type = "option"; })", cwd());
  nix::Value V;
  State->eval(AST, V);

  ASSERT_FALSE(isDerivation(*State, V));
}

TEST_F(ValueTest, IsDerivation_pos) {
  nix::Expr *AST =
      State->parseExprFromString(R"({ type = "derivation"; })", cwd());
  nix::Value V;
  State->eval(AST, V);

  ASSERT_TRUE(isDerivation(*State, V));
}

} // namespace
