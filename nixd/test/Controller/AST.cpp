#include <gtest/gtest.h>

#include "AST.h"

#include <nixf/Parse/Parser.h>

using namespace nixd;

namespace {

struct ASTTest : testing::Test {
  std::vector<nixf::Diagnostic> Diags;
  nixf::ParentMapAnalysis PM;
};

TEST_F(ASTTest, AttrPath) {
  auto AST = nixf::parse("{ a.b.c.d = 1; }", Diags);
  ASSERT_TRUE(AST);
  PM.runOnAST(*AST);
  const nixf::Node *Desc = AST->descend({{0, 6}, {0, 6}});
  ASSERT_EQ(Desc->kind(), nixf::Node::NK_Identifer);
  ASSERT_EQ(PM.query(*Desc)->kind(), nixf::Node::NK_AttrName);
  auto Vec = getSelectAttrPath(
      static_cast<const nixf::AttrName &>(*PM.query(*Desc)), PM);
  ASSERT_EQ(Vec.size(), 3);
  ASSERT_EQ(Vec[0], "a");
  ASSERT_EQ(Vec[1], "b");
  ASSERT_EQ(Vec[2], "c");
}

TEST_F(ASTTest, ValueAttrPath) {
  auto AST = nixf::parse("{ a.b.c.d = 1; }", Diags);
  ASSERT_TRUE(AST);
  PM.runOnAST(*AST);
  const nixf::Node *Desc = AST->descend({{0, 12}, {0, 12}});
  const nixf::Node *Up = PM.upExpr(*Desc);
  ASSERT_EQ(Up->kind(), nixf::Node::NK_ExprInt);
  auto Vec = getValueAttrPath(*Up, PM);
  ASSERT_EQ(Vec.size(), 4);
  ASSERT_EQ(Vec[0], "a");
  ASSERT_EQ(Vec[1], "b");
  ASSERT_EQ(Vec[2], "c");
  ASSERT_EQ(Vec[3], "d");
}

TEST_F(ASTTest, ValueAttrPathNestedComplex) {
  auto AST = nixf::parse("{ yy.zz = { a.b.c.d = 1; }; }", Diags);
  ASSERT_TRUE(AST);
  PM.runOnAST(*AST);
  const nixf::Node *Desc = AST->descend({{0, 23}, {0, 23}});
  const nixf::Node *Up = PM.upExpr(*Desc);
  ASSERT_EQ(Up->kind(), nixf::Node::NK_ExprInt);
  auto Vec = getValueAttrPath(*Up, PM);
  ASSERT_EQ(Vec.size(), 6);
  ASSERT_EQ(Vec[0], "yy");
  ASSERT_EQ(Vec[1], "zz");
  ASSERT_EQ(Vec[2], "a");
  ASSERT_EQ(Vec[3], "b");
}

TEST_F(ASTTest, ValueAttrPathTop) {
  auto AST = nixf::parse("{ yy.zz = { a.b.c.d = 1; }; }", Diags);
  ASSERT_TRUE(AST);
  PM.runOnAST(*AST);
  const nixf::Node *Desc = AST->descend({{0, 0}, {0, 23}});
  ASSERT_EQ(Desc->kind(), nixf::Node::NK_ExprAttrs);
  auto Vec = getValueAttrPath(*Desc, PM);
  ASSERT_EQ(Vec.size(), 0);
}

} // namespace
