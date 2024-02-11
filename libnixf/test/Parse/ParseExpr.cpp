#include <gtest/gtest.h>

#include "Parser.h"

namespace {

using namespace nixf;
using namespace std::string_view_literals;

TEST(Parser, SelectOK) {
  auto Src = R"(a.b.c)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExprSelect();

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_EQ(AST->kind(), Node::NK_ExprSelect);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 5, 5));

  const auto &S = static_cast<ExprSelect *>(AST.get());

  ASSERT_EQ(S->defaultExpr(), nullptr);
  ASSERT_TRUE(S->path()->lCur().isAt(0, 2, 2));
  ASSERT_TRUE(S->path()->rCur().isAt(0, 5, 5));
}

TEST(Parser, SelectSimple) {
  auto Src = R"(1)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExprSelect();

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_EQ(AST->kind(), Node::NK_ExprInt);
}

TEST(Parser, SelectDefault) {
  auto Src = R"(a.c or b)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExprSelect();

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_EQ(AST->kind(), Node::NK_ExprSelect);

  const auto &S = static_cast<ExprSelect *>(AST.get());

  ASSERT_TRUE(S->defaultExpr()->range().lCur().isAt(0, 7, 7));
  ASSERT_TRUE(S->defaultExpr()->range().rCur().isAt(0, 8, 8));
  ASSERT_EQ(S->defaultExpr()->kind(), Node::NK_ExprVar);
}

TEST(Parser, SelectExtraDot) {
  auto Src = R"(a.)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExprSelect();

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 1);
  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_SelectExtraDot);
  ASSERT_TRUE(Diags[0].range().lCur().isAt(0, 1, 1));
  ASSERT_TRUE(Diags[0].range().rCur().isAt(0, 2, 2));

  // Check the note.
  ASSERT_EQ(Diags[0].notes().size(), 0);

  // Check fix-it hints.
  ASSERT_EQ(Diags[0].fixes().size(), 2);
  ASSERT_EQ(Diags[0].fixes()[0].edits().size(), 1);
  ASSERT_EQ(Diags[0].fixes()[0].message(), "remove extra .");
  const auto &F = Diags[0].fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().lCur().isAt(0, 1, 1));
  ASSERT_TRUE(F.oldRange().rCur().isAt(0, 2, 2));

  ASSERT_EQ(Diags[0].fixes()[1].edits().size(), 1);
  ASSERT_EQ(Diags[0].fixes()[1].message(), "insert dummy attrpath");
  const auto &F2 = Diags[0].fixes()[1].edits()[0];
  ASSERT_TRUE(F2.oldRange().lCur().isAt(0, 2, 2));
  ASSERT_TRUE(F2.oldRange().rCur().isAt(0, 2, 2));
  ASSERT_EQ(F2.newText(), "\"dummy\"");
}

TEST(Parser, SelectExtraOr) {
  auto Src = R"(a.c or)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExprSelect();

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 1);
  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_Expected);

  // Check the note.
  ASSERT_EQ(Diags[0].notes().size(), 0);

  // Check fix-it hints.
  ASSERT_EQ(Diags[0].fixes().size(), 2);
  ASSERT_EQ(Diags[0].fixes()[1].edits().size(), 1);
  ASSERT_EQ(Diags[0].fixes()[1].message(), "remove `or` keyword");
  const auto &F = Diags[0].fixes()[1].edits()[0];
  ASSERT_TRUE(F.oldRange().lCur().isAt(0, 4, 4));
  ASSERT_TRUE(F.oldRange().rCur().isAt(0, 6, 6));
}

TEST(Parser, ParseExprApp) {
  auto Src = R"(a b)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExprApp();

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_EQ(AST->kind(), Node::NK_ExprCall);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 3, 3));

  const auto &A = static_cast<ExprCall *>(AST.get());

  ASSERT_TRUE(A->fn().range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(A->fn().range().rCur().isAt(0, 1, 1));
  ASSERT_EQ(A->fn().kind(), Node::NK_ExprVar);

  ASSERT_EQ(A->args().size(), 1);
}

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

TEST(Parser, ExprIf) {
  // if ... then ... else ...
  auto Src = R"(if 1 then cc else "d")"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExpr();

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprIf);

  ASSERT_TRUE(AST->lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->rCur().isAt(0, 21, 21));

  ExprIf &If = *static_cast<ExprIf *>(AST.get());

  ASSERT_EQ(If.cond()->kind(), Node::NK_ExprInt);
  ASSERT_EQ(If.then()->kind(), Node::NK_ExprVar);
  ASSERT_EQ(If.elseExpr()->kind(), Node::NK_ExprString);
}

} // namespace
