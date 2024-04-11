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

TEST_F(ValueTest, selectAttrPath) {
  nix::Expr *AST = State->parseExprFromString(R"({ a.b.c.d = 1; })", cwd());
  nix::Value V;
  State->eval(AST, V);

  nix::Value &Nested = selectStringViews(*State, V, {"a", "b"});

  // Make sure no extra "force" performed.
  ASSERT_EQ(Nested.type(), nix::ValueType::nThunk);

  nix::Value &Kern = selectStringViews(*State, Nested, {"c", "d"});

  ASSERT_EQ(Kern.type(), nix::ValueType::nInt);
  ASSERT_EQ(Kern.integer, 1);
}

} // namespace
