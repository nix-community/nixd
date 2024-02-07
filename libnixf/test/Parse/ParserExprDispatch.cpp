#include <gtest/gtest.h>

#include "Parser.h"

namespace {

using namespace nixf;
using namespace std::string_view_literals;

TEST(Parser, ExprDispatch_id_at) {
  // ID @
  auto Src = R"(a @)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExpr();

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprLambda);
}

TEST(Parser, ExprDispatch_id_colon) {
  // ID :
  auto Src = R"(a :)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExpr();

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprLambda);
}

TEST(Parser, ExprDispatch_r_curly_id_colon) {
  // { ID :
  auto Src = R"({ a :)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExpr();

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprLambda);
}

TEST(Parser, ExprDispatch_r_curly_id_at) {
  // { ID @
  auto Src = R"({ a @)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExpr();

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprLambda);
}

TEST(Parser, ExprDispatch_r_curly_id_question) {
  // { ID ?
  auto Src = R"({ a ?)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExpr();

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprLambda);
}
TEST(Parser, ExprDispatch_r_curly_id_comma) {
  // { ID ,
  auto Src = R"({ a ,)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExpr();

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprLambda);
}
TEST(Parser, ExprDispatch_r_curly_id_id) {
  // { ID ID
  auto Src = R"({ a b)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExpr();

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprLambda);
}

TEST(Parser, ExprDispatch_r_curly_id_ellipsis) {
  // { ID ...
  auto Src = R"({ a ...)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExpr();

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprLambda);
}

} // namespace
