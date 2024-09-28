#include <gtest/gtest.h>

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Parse/Parser.h"
#include "nixf/Sema/ParentMap.h"

using namespace nixf;

namespace {

struct ParentMapTest : testing::Test {
  std::vector<Diagnostic> Diags;

  ParentMapAnalysis PMA;
};

TEST_F(ParentMapTest, Basic) {
  auto AST = nixf::parse(R"(a)", Diags);
  PMA.runOnAST(*AST);

  const Node *ID = AST->descend({{0, 0}, {0, 1}});
  ASSERT_EQ(ID->kind(), Node::NK_Identifier);

  const Node *Var = PMA.upExpr(*ID);

  ASSERT_EQ(Var->kind(), Node::NK_ExprVar);
}

} // namespace
