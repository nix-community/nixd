#include <gtest/gtest.h>

#include "Parser.h"

#include "nixf/Parse/Parser.h"

namespace {

using namespace nixf;
using namespace std::literals;

TEST(Parser, Integer) {
  auto Src = "1"sv;
  std::vector<Diagnostic> Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprInt);
  ASSERT_EQ(static_cast<ExprInt *>(Expr.get())->value(), 1);
}

TEST(Parser, Float) {
  auto Src = "1.0"sv;
  std::vector<Diagnostic> Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprFloat);
  ASSERT_EQ(static_cast<ExprFloat *>(Expr.get())->value(), 1.0);
  ASSERT_EQ(Diags.size(), 0);
  ASSERT_TRUE(Expr->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().rCur().isAt(0, 3, 3));
}

TEST(Parser, FloatLeading) {
  auto Src = "01.0"sv;
  std::vector<Diagnostic> Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprFloat);
  ASSERT_EQ(static_cast<ExprFloat *>(Expr.get())->value(), 1.0);

  // Check the diagnostic.
  ASSERT_EQ(Diags.size(), 1);
  auto &D = Diags[0];
  ASSERT_TRUE(Expr->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().rCur().isAt(0, 4, 4));
  ASSERT_EQ(D.kind(), Diagnostic::DK_FloatLeadingZero);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], "01");
}

TEST(Parser, FloatLeading00) {
  auto Src = "00.5"sv;
  std::vector<Diagnostic> Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprFloat);
  ASSERT_EQ(static_cast<ExprFloat *>(Expr.get())->value(), 0.5);
  ASSERT_EQ(Diags.size(), 1);
  ASSERT_TRUE(Expr->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().rCur().isAt(0, 4, 4));
}

TEST(Parser, StringSimple) {
  auto Src = R"("aaa")"sv;
  std::vector<Diagnostic> Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprString);
  const auto &Parts = static_cast<ExprString *>(Expr.get())->parts();
  ASSERT_TRUE(Parts.range().lCur().isAt(0, 1, 1));
  ASSERT_TRUE(Parts.range().rCur().isAt(0, 4, 4));
  ASSERT_EQ(Parts.fragments().size(), 1);
  ASSERT_EQ(Diags.size(), 0);
  ASSERT_TRUE(Expr->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().rCur().isAt(0, 5, 5));
}

TEST(Parser, StringMissingDQuote) {
  auto Src = R"("aaa)"sv;
  std::vector<Diagnostic> Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprString);
  const auto &Parts = static_cast<ExprString *>(Expr.get())->parts();
  ASSERT_TRUE(Parts.range().lCur().isAt(0, 1, 1));
  ASSERT_TRUE(Parts.range().rCur().isAt(0, 4, 4));
  ASSERT_EQ(Parts.fragments().size(), 1);

  // Check the diagnostic.
  ASSERT_EQ(Diags.size(), 1);
  auto &D = Diags[0];
  ASSERT_TRUE(Expr->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().rCur().isAt(0, 4, 4));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], "\"");

  // Check the note.
  ASSERT_EQ(D.notes().size(), 1);
  const auto &N = D.notes()[0];
  ASSERT_TRUE(N.range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(N.range().rCur().isAt(0, 1, 1));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "\"");

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "insert \"");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().lCur().isAt(0, 4, 4));
  ASSERT_TRUE(F.oldRange().rCur().isAt(0, 4, 4));
  ASSERT_EQ(F.newText(), "\"");
}

TEST(Parser, StringInterpolation) {
  auto Src = R"("aaa ${1} bbb")"sv;
  std::vector<Diagnostic> Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprString);
  const auto &Parts = static_cast<ExprString *>(Expr.get())->parts();
  ASSERT_TRUE(Parts.range().lCur().isAt(0, 1, 1));
  ASSERT_TRUE(Parts.range().rCur().isAt(0, 13, 13));
  ASSERT_EQ(Parts.fragments().size(), 3);

  ASSERT_EQ(Parts.fragments()[0].kind(), InterpolablePart::SPK_Escaped);
  ASSERT_EQ(Parts.fragments()[1].kind(), InterpolablePart::SPK_Interpolation);
  ASSERT_EQ(Parts.fragments()[2].kind(), InterpolablePart::SPK_Escaped);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_TRUE(Expr->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().rCur().isAt(0, 14, 14));
}

TEST(Parser, IndentedString) {
  auto Src = R"(''aaa
  foo

  bar

  ${''string''}

  )"sv;

  std::vector<Diagnostic> Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprString);
  const auto &Parts = static_cast<ExprString *>(Expr.get())->parts();
  ASSERT_EQ(Parts.fragments().size(), 3);

  // Check the diagnostic.
  ASSERT_EQ(Diags.size(), 1);
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(7, 2, 39));
  ASSERT_TRUE(D.range().rCur().isAt(7, 2, 39));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], "''");

  // Check the note.
  ASSERT_EQ(D.notes().size(), 1);
  const auto &N = D.notes()[0];
  ASSERT_TRUE(N.range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(N.range().rCur().isAt(0, 2, 2));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "''");

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "insert ''");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().lCur().isAt(7, 2, 39));
  ASSERT_TRUE(F.oldRange().rCur().isAt(7, 2, 39));
  ASSERT_EQ(F.newText(), "''");
}

TEST(Parser, InterpolationOK) {
  auto Src = R"("${1}")"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprString);
  const auto &Parts = static_cast<ExprString *>(AST.get())->parts();
  ASSERT_EQ(Parts.fragments().size(), 1);
  ASSERT_EQ(Parts.fragments()[0].kind(), InterpolablePart::SPK_Interpolation);
  ASSERT_EQ(Diags.size(), 0);

  // Check the interpolation range
  const auto &I = Parts.fragments()[0].interpolation();
  ASSERT_TRUE(I.range().lCur().isAt(0, 1, 1));
  ASSERT_TRUE(I.range().rCur().isAt(0, 5, 5));
}

TEST(Parser, InterpolationNoRCurly) {
  auto Src = R"("${1")"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprString);
  const auto &Parts = static_cast<ExprString *>(AST.get())->parts();
  ASSERT_EQ(Parts.fragments().size(), 1);
  ASSERT_EQ(Parts.fragments()[0].kind(), InterpolablePart::SPK_Interpolation);
  ASSERT_EQ(Diags.size(), 1);

  // Check the diagnostic.
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(0, 4, 4));
  ASSERT_TRUE(D.range().rCur().isAt(0, 4, 4));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], "}");

  // Check the note.
  ASSERT_EQ(D.notes().size(), 1);
  const auto &N = D.notes()[0];
  ASSERT_TRUE(N.range().lCur().isAt(0, 1, 1));
  ASSERT_TRUE(N.range().rCur().isAt(0, 3, 3));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "${");

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "insert }");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().lCur().isAt(0, 4, 4));
  ASSERT_TRUE(F.oldRange().rCur().isAt(0, 4, 4));
  ASSERT_EQ(F.newText(), "}");
}

TEST(Parser, InterpolationNullExpr) {
  auto Src = R"("${}")"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprString);
  const auto &Parts = static_cast<ExprString *>(AST.get())->parts();
  ASSERT_EQ(Parts.fragments().size(), 1);

  // Check the diagnostic.
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(0, 3, 3));
  ASSERT_TRUE(D.range().rCur().isAt(0, 3, 3));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], "interpolation expression");

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "insert dummy expression");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().lCur().isAt(0, 3, 3));
  ASSERT_TRUE(F.oldRange().rCur().isAt(0, 3, 3));
  ASSERT_EQ(F.newText(), " expr");
}

TEST(Parser, PathOK) {
  auto Src = R"(a/b/c/${"foo"}/d)"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprPath);
  const auto &Parts = static_cast<ExprPath *>(AST.get())->parts();
  ASSERT_EQ(Parts.fragments().size(), 3);

  // Check the AST range
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 16, 16));

  // Check parts range. Should be the same as AST.
  ASSERT_TRUE(Parts.range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(Parts.range().rCur().isAt(0, 16, 16));

  // Check the first part.
  ASSERT_EQ(Parts.fragments()[0].kind(), InterpolablePart::SPK_Escaped);
  ASSERT_EQ(Parts.fragments()[0].escaped(), "a/b/c/");

  // Check the second part.
  ASSERT_EQ(Parts.fragments()[1].kind(), InterpolablePart::SPK_Interpolation);

  // Check the third part.
  ASSERT_EQ(Parts.fragments()[2].kind(), InterpolablePart::SPK_Escaped);
  ASSERT_EQ(Parts.fragments()[2].escaped(), "/d");
}

TEST(Parser, ParenExpr) {
  auto Src = R"((1))"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprParen);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 3, 3));

  // Also check the location of parenthesis.
  auto &P = *static_cast<ExprParen *>(AST.get());
  ASSERT_TRUE(P.lparen()->lCur().isAt(0, 0, 0));
  ASSERT_TRUE(P.lparen()->rCur().isAt(0, 1, 1));
  ASSERT_TRUE(P.rparen()->lCur().isAt(0, 2, 2));
  ASSERT_TRUE(P.rparen()->rCur().isAt(0, 3, 3));
}

TEST(Parser, ParenExprMissingRParen) {
  auto Src = R"((1)"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprParen);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 2, 2));

  // Also check the location of parenthesis.
  auto &P = *static_cast<ExprParen *>(AST.get());
  ASSERT_TRUE(P.lparen()->lCur().isAt(0, 0, 0));
  ASSERT_TRUE(P.lparen()->rCur().isAt(0, 1, 1));
  ASSERT_EQ(P.rparen(), nullptr);

  // Check the diagnostic.
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(0, 2, 2));
  ASSERT_TRUE(D.range().rCur().isAt(0, 2, 2));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], ")");

  // Check the note.
  ASSERT_EQ(D.notes().size(), 1);
  const auto &N = D.notes()[0];
  ASSERT_TRUE(N.range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(N.range().rCur().isAt(0, 1, 1));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "(");

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "insert )");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().lCur().isAt(0, 2, 2));
  ASSERT_TRUE(F.oldRange().rCur().isAt(0, 2, 2));
  ASSERT_EQ(F.newText(), ")");
}

TEST(Parser, ParenNullExpr) {
  auto Src = R"(())"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprParen);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 2, 2));

  // Also check the location of parenthesis.
  auto &P = *static_cast<ExprParen *>(AST.get());
  ASSERT_TRUE(P.lparen()->lCur().isAt(0, 0, 0));
  ASSERT_TRUE(P.lparen()->rCur().isAt(0, 1, 1));
  ASSERT_TRUE(P.rparen()->lCur().isAt(0, 1, 1));
  ASSERT_TRUE(P.rparen()->rCur().isAt(0, 2, 2));

  // Check the diagnostic.
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().lCur().isAt(0, 1, 1));
  ASSERT_TRUE(D.range().rCur().isAt(0, 1, 1));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], "parenthesized expression");

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "insert dummy expression");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().lCur().isAt(0, 1, 1));
  ASSERT_TRUE(F.oldRange().rCur().isAt(0, 1, 1));
  ASSERT_EQ(F.newText(), " expr");
}

TEST(Parser, ParseExprList) {
  auto Src = R"([1 2 3])"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExprList();

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_EQ(AST->kind(), Node::NK_ExprList);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 7, 7));

  const auto &L = static_cast<ExprList *>(AST.get());

  ASSERT_EQ(L->elements().size(), 3);
  ASSERT_TRUE(L->elements()[0]->range().lCur().isAt(0, 1, 1));
  ASSERT_TRUE(L->elements()[0]->range().rCur().isAt(0, 2, 2));
  ASSERT_EQ(L->elements()[0]->kind(), Node::NK_ExprInt);
}

TEST(Parser, ParseExprListMissingRBracket) {
  auto Src = R"([1 2 3)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExprList();

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 1);
  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_Expected);
  ASSERT_TRUE(Diags[0].range().lCur().isAt(0, 6, 6));
  ASSERT_TRUE(Diags[0].range().rCur().isAt(0, 6, 6));
  ASSERT_EQ(Diags[0].args().size(), 1);
  ASSERT_EQ(Diags[0].args()[0], "]");

  // Check the note.
  ASSERT_EQ(Diags[0].notes().size(), 1);
  const auto &N = Diags[0].notes()[0];
  ASSERT_TRUE(N.range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(N.range().rCur().isAt(0, 1, 1));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "[");

  // Check fix-it hints.
  ASSERT_EQ(Diags[0].fixes().size(), 1);
  ASSERT_EQ(Diags[0].fixes()[0].edits().size(), 1);
  ASSERT_EQ(Diags[0].fixes()[0].message(), "insert ]");
  const auto &F = Diags[0].fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().lCur().isAt(0, 6, 6));
  ASSERT_TRUE(F.oldRange().rCur().isAt(0, 6, 6));
  ASSERT_EQ(F.newText(), "]");
}

} // namespace
