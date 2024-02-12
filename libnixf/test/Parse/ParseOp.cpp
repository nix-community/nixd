#include <gtest/gtest.h>

#include "Parser.h"

#include "nixf/Basic/Nodes/Op.h"

namespace {

using namespace nixf;
using namespace std::string_view_literals;

TEST(Parser, ExprOp) {
  auto Src = R"(1 + 2 * 3 + 2.1)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExpr();

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprBinOp);

  auto &BinOp = *static_cast<ExprBinOp *>(AST.get());

  ASSERT_TRUE(BinOp.lhs());
  ASSERT_TRUE(BinOp.rhs());
  ASSERT_EQ(BinOp.rhs()->kind(), Node::NK_ExprFloat);
  ASSERT_EQ(BinOp.lhs()->kind(), Node::NK_ExprBinOp);
}

TEST(Parser, UnaryOp) {
  auto Src = R"(!stdenv.isDarwin)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExpr();

  ASSERT_TRUE(AST);

  ASSERT_EQ(AST->kind(), Node::NK_ExprUnaryOp);
  auto &UnaryOp = *static_cast<ExprUnaryOp *>(AST.get());

  ASSERT_EQ(UnaryOp.expr()->kind(), Node::NK_ExprSelect);
}

TEST(Parser, OpHasAttr) {
  auto Src = R"(a ? b.c.d)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExpr();

  ASSERT_TRUE(AST);

  ASSERT_EQ(AST->kind(), Node::NK_ExprOpHasAttr);

  ASSERT_EQ(Diags.size(), 0);
}

TEST(Parser, OpHasAttr_empty) {
  auto Src = R"(a ?)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExpr();

  ASSERT_TRUE(AST);

  ASSERT_EQ(AST->kind(), Node::NK_ExprOpHasAttr);

  // libnixf accepts emtpy attrpath while parsing.
  ASSERT_EQ(Diags.size(), 0);
}

} // namespace
