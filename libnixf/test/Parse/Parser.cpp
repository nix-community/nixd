#include <gtest/gtest.h>

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Range.h"
#include "nixf/Parse/Nodes.h"
#include "nixf/Parse/Parser.h"

#include <string_view>

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
  ASSERT_TRUE(Expr->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().end().isAt(0, 3, 3));
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
  ASSERT_TRUE(Expr->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().end().isAt(0, 4, 4));
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
  ASSERT_TRUE(Expr->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().end().isAt(0, 4, 4));
}

TEST(Parser, StringSimple) {
  auto Src = R"("aaa")"sv;
  std::vector<Diagnostic> Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprString);
  auto Parts = static_cast<ExprString *>(Expr.get())->parts();
  ASSERT_TRUE(Parts->range().begin().isAt(0, 1, 1));
  ASSERT_TRUE(Parts->range().end().isAt(0, 4, 4));
  ASSERT_EQ(Parts->fragments().size(), 1);
  ASSERT_EQ(Diags.size(), 0);
  ASSERT_TRUE(Expr->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().end().isAt(0, 5, 5));
}

TEST(Parser, StringMissingDQuote) {
  auto Src = R"("aaa)"sv;
  std::vector<Diagnostic> Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprString);
  auto Parts = static_cast<ExprString *>(Expr.get())->parts();
  ASSERT_TRUE(Parts->range().begin().isAt(0, 1, 1));
  ASSERT_TRUE(Parts->range().end().isAt(0, 4, 4));
  ASSERT_EQ(Parts->fragments().size(), 1);

  // Check the diagnostic.
  ASSERT_EQ(Diags.size(), 1);
  auto &D = Diags[0];
  ASSERT_TRUE(Expr->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().end().isAt(0, 4, 4));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], "\"");

  // Check the note.
  ASSERT_EQ(D.notes().size(), 1);
  auto &N = D.notes()[0];
  ASSERT_TRUE(N.range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(N.range().end().isAt(0, 1, 1));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "\"");

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "insert \"");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().begin().isAt(0, 4, 4));
  ASSERT_TRUE(F.oldRange().end().isAt(0, 4, 4));
  ASSERT_EQ(F.newText(), "\"");
}

TEST(Parser, StringInterpolation) {
  auto Src = R"("aaa ${1} bbb")"sv;
  std::vector<Diagnostic> Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprString);
  auto Parts = static_cast<ExprString *>(Expr.get())->parts();
  ASSERT_TRUE(Parts->range().begin().isAt(0, 1, 1));
  ASSERT_TRUE(Parts->range().end().isAt(0, 13, 13));
  ASSERT_EQ(Parts->fragments().size(), 3);

  ASSERT_EQ(Parts->fragments()[0].kind(), InterpolablePart::SPK_Escaped);
  ASSERT_EQ(Parts->fragments()[1].kind(), InterpolablePart::SPK_Interpolation);
  ASSERT_EQ(Parts->fragments()[2].kind(), InterpolablePart::SPK_Escaped);

  ASSERT_EQ(Diags.size(), 0);
  ASSERT_TRUE(Expr->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().end().isAt(0, 14, 14));
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
  auto Parts = static_cast<ExprString *>(Expr.get())->parts();
  ASSERT_EQ(Parts->fragments().size(), 3);

  // Check the diagnostic.
  ASSERT_EQ(Diags.size(), 1);
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().begin().isAt(7, 2, 39));
  ASSERT_TRUE(D.range().end().isAt(7, 2, 39));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], "''");

  // Check the note.
  ASSERT_EQ(D.notes().size(), 1);
  auto &N = D.notes()[0];
  ASSERT_TRUE(N.range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(N.range().end().isAt(0, 2, 2));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "''");

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "insert ''");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().begin().isAt(7, 2, 39));
  ASSERT_TRUE(F.oldRange().end().isAt(7, 2, 39));
  ASSERT_EQ(F.newText(), "''");
}

TEST(Parser, InterpolationOK) {
  auto Src = R"("${1}")"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprString);
  auto Parts = static_cast<ExprString *>(AST.get())->parts();
  ASSERT_EQ(Parts->fragments().size(), 1);
  ASSERT_EQ(Parts->fragments()[0].kind(), InterpolablePart::SPK_Interpolation);
  ASSERT_EQ(Diags.size(), 0);

  // Check the interpolation range
  const std::shared_ptr<Expr> &I = Parts->fragments()[0].interpolation();
  ASSERT_TRUE(I->range().begin().isAt(0, 3, 3));
  ASSERT_TRUE(I->range().end().isAt(0, 4, 4));
}

TEST(Parser, InterpolationNoRCurly) {
  auto Src = R"("${1")"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprString);
  auto Parts = static_cast<ExprString *>(AST.get())->parts();
  ASSERT_EQ(Parts->fragments().size(), 1);
  ASSERT_EQ(Parts->fragments()[0].kind(), InterpolablePart::SPK_Interpolation);
  ASSERT_EQ(Diags.size(), 1);

  // Check the diagnostic.
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().begin().isAt(0, 4, 4));
  ASSERT_TRUE(D.range().end().isAt(0, 4, 4));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], "}");

  // Check the note.
  ASSERT_EQ(D.notes().size(), 1);
  auto &N = D.notes()[0];
  ASSERT_TRUE(N.range().begin().isAt(0, 1, 1));
  ASSERT_TRUE(N.range().end().isAt(0, 3, 3));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "${");

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "insert }");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().begin().isAt(0, 4, 4));
  ASSERT_TRUE(F.oldRange().end().isAt(0, 4, 4));
  ASSERT_EQ(F.newText(), "}");
}

TEST(Parser, InterpolationNullExpr) {
  auto Src = R"("${}")"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprString);
  auto Parts = static_cast<ExprString *>(AST.get())->parts();
  ASSERT_EQ(Parts->fragments().size(), 0);

  // Check the diagnostic.
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().begin().isAt(0, 3, 3));
  ASSERT_TRUE(D.range().end().isAt(0, 3, 3));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], "interpolation expression");

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "insert dummy expression");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().begin().isAt(0, 3, 3));
  ASSERT_TRUE(F.oldRange().end().isAt(0, 3, 3));
  ASSERT_EQ(F.newText(), " expr");
}

TEST(Parser, PathOK) {
  auto Src = R"(a/b/c/${"foo"}/d)"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprPath);
  auto Parts = static_cast<ExprPath *>(AST.get())->parts();
  ASSERT_EQ(Parts->fragments().size(), 3);

  // Check the AST range
  ASSERT_TRUE(AST->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().end().isAt(0, 16, 16));

  // Check parts range. Should be the same as AST.
  ASSERT_TRUE(Parts->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(Parts->range().end().isAt(0, 16, 16));

  // Check the first part.
  ASSERT_EQ(Parts->fragments()[0].kind(), InterpolablePart::SPK_Escaped);
  ASSERT_EQ(Parts->fragments()[0].escaped(), "a/b/c/");

  // Check the second part.
  ASSERT_EQ(Parts->fragments()[1].kind(), InterpolablePart::SPK_Interpolation);

  // Check the third part.
  ASSERT_EQ(Parts->fragments()[2].kind(), InterpolablePart::SPK_Escaped);
  ASSERT_EQ(Parts->fragments()[2].escaped(), "/d");
}

TEST(Parser, ParenExpr) {
  auto Src = R"((1))"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprParen);
  ASSERT_TRUE(AST->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().end().isAt(0, 3, 3));

  // Also check the location of parenthesis.
  auto &P = *static_cast<ExprParen *>(AST.get());
  ASSERT_TRUE(P.lparen()->begin().isAt(0, 0, 0));
  ASSERT_TRUE(P.lparen()->end().isAt(0, 1, 1));
  ASSERT_TRUE(P.rparen()->begin().isAt(0, 2, 2));
  ASSERT_TRUE(P.rparen()->end().isAt(0, 3, 3));
}

TEST(Parser, ParenExprMissingRParen) {
  auto Src = R"((1)"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprParen);
  ASSERT_TRUE(AST->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().end().isAt(0, 2, 2));

  // Also check the location of parenthesis.
  auto &P = *static_cast<ExprParen *>(AST.get());
  ASSERT_TRUE(P.lparen()->begin().isAt(0, 0, 0));
  ASSERT_TRUE(P.lparen()->end().isAt(0, 1, 1));
  ASSERT_EQ(P.rparen(), nullptr);

  // Check the diagnostic.
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().begin().isAt(0, 2, 2));
  ASSERT_TRUE(D.range().end().isAt(0, 2, 2));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], ")");

  // Check the note.
  ASSERT_EQ(D.notes().size(), 1);
  auto &N = D.notes()[0];
  ASSERT_TRUE(N.range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(N.range().end().isAt(0, 1, 1));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "(");

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "insert )");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().begin().isAt(0, 2, 2));
  ASSERT_TRUE(F.oldRange().end().isAt(0, 2, 2));
  ASSERT_EQ(F.newText(), ")");
}

TEST(Parser, ParenNullExpr) {
  auto Src = R"(())"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);
  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprParen);
  ASSERT_TRUE(AST->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().end().isAt(0, 2, 2));

  // Also check the location of parenthesis.
  auto &P = *static_cast<ExprParen *>(AST.get());
  ASSERT_TRUE(P.lparen()->begin().isAt(0, 0, 0));
  ASSERT_TRUE(P.lparen()->end().isAt(0, 1, 1));
  ASSERT_TRUE(P.rparen()->begin().isAt(0, 1, 1));
  ASSERT_TRUE(P.rparen()->end().isAt(0, 2, 2));

  // Check the diagnostic.
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().begin().isAt(0, 1, 1));
  ASSERT_TRUE(D.range().end().isAt(0, 1, 1));
  ASSERT_EQ(D.kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D.args().size(), 1);
  ASSERT_EQ(D.args()[0], "parenthesized expression");

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "insert dummy expression");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().begin().isAt(0, 1, 1));
  ASSERT_TRUE(F.oldRange().end().isAt(0, 1, 1));
  ASSERT_EQ(F.newText(), " expr");
}

TEST(Parser, AttrsOK) {
  auto Src = R"({})"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().end().isAt(0, 2, 2));
  ASSERT_FALSE(static_cast<ExprAttrs *>(AST.get())->isRecursive());

  ASSERT_EQ(Diags.size(), 0);
}

TEST(Parser, RecAttrsOK) {
  auto Src = R"(rec { })"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().end().isAt(0, 7, 7));
  ASSERT_TRUE(static_cast<ExprAttrs *>(AST.get())->isRecursive());

  ASSERT_EQ(Diags.size(), 0);
}

TEST(Parser, RecAttrsMissingLCurly) {
  auto Src = R"(rec )"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().end().isAt(0, 3, 3));
  ASSERT_TRUE(static_cast<ExprAttrs *>(AST.get())->isRecursive());

  ASSERT_EQ(Diags.size(), 2);
  auto &D = Diags;
  ASSERT_TRUE(D[0].range().begin().isAt(0, 3, 3));
  ASSERT_TRUE(D[0].range().end().isAt(0, 3, 3));
  ASSERT_EQ(D[0].kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D[0].args().size(), 1);
  ASSERT_EQ(D[0].args()[0], "{");
  ASSERT_TRUE(D[0].range().begin().isAt(0, 3, 3));
  ASSERT_TRUE(D[0].range().end().isAt(0, 3, 3));
  ASSERT_EQ(D[1].kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D[1].args().size(), 1);
  ASSERT_EQ(D[1].args()[0], "}");

  // Check the note.
  ASSERT_EQ(D[0].notes().size(), 0);
  ASSERT_EQ(D[1].notes().size(), 1);
  const auto &N = D[1].notes()[0];
  ASSERT_TRUE(N.range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(N.range().end().isAt(0, 3, 3));
  ASSERT_EQ(N.kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N.args().size(), 1);
  ASSERT_EQ(N.args()[0], "rec");

  // Check fix-it hints.
  ASSERT_EQ(D[0].fixes().size(), 1);
  ASSERT_EQ(D[0].fixes()[0].edits().size(), 1);
  ASSERT_EQ(D[0].fixes()[0].message(), "insert {");
  const auto &F = D[0].fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().begin().isAt(0, 3, 3));
  ASSERT_TRUE(F.oldRange().end().isAt(0, 3, 3));
  ASSERT_EQ(F.newText(), "{");

  ASSERT_EQ(D[1].fixes().size(), 1);
  ASSERT_EQ(D[1].fixes()[0].edits().size(), 1);
  ASSERT_EQ(D[1].fixes()[0].message(), "insert }");
  const auto &F2 = D[1].fixes()[0].edits()[0];
  ASSERT_TRUE(F2.oldRange().begin().isAt(0, 3, 3));
  ASSERT_TRUE(F2.oldRange().end().isAt(0, 3, 3));
  ASSERT_EQ(F2.newText(), "}");
}

TEST(Parser, AttrsOrID) {
  auto Src = R"({ or = 1; })"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().end().isAt(0, 11, 11));
  ASSERT_FALSE(static_cast<ExprAttrs *>(AST.get())->isRecursive());

  ASSERT_EQ(Diags.size(), 1);
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().begin().isAt(0, 2, 2));
  ASSERT_TRUE(D.range().end().isAt(0, 4, 4));
  ASSERT_EQ(D.kind(), Diagnostic::DK_OrIdentifier);
}

TEST(Parser, AttrsOKSpecialAttr) {
  auto Src = R"({ a.b."foo".${"bar"} = 1; })"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().end().isAt(0, 27, 27));

  ASSERT_EQ(Diags.size(), 0);
}

TEST(Parser, AttrsExtraDot) {
  auto Src = R"({ a.b. = 1; })"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprAttrs);
  ASSERT_TRUE(AST->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().end().isAt(0, 13, 13));

  ASSERT_EQ(Diags.size(), 1);
  auto &D = Diags[0];
  ASSERT_TRUE(D.range().begin().isAt(0, 5, 5));
  ASSERT_TRUE(D.range().end().isAt(0, 6, 6));
  ASSERT_EQ(D.kind(), Diagnostic::DK_AttrPathExtraDot);

  // Check that the note is correct.
  ASSERT_EQ(D.notes().size(), 0);

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 2);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "remove extra .");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().begin().isAt(0, 5, 5));
  ASSERT_TRUE(F.oldRange().end().isAt(0, 6, 6));
  ASSERT_EQ(F.newText(), "");

  ASSERT_EQ(D.fixes()[1].edits().size(), 1);
  ASSERT_EQ(D.fixes()[1].message(), "insert dummy attrname");
  const auto &F2 = D.fixes()[1].edits()[0];
  ASSERT_TRUE(F2.oldRange().begin().isAt(0, 6, 6));
  ASSERT_TRUE(F2.oldRange().end().isAt(0, 6, 6));
  ASSERT_EQ(F2.newText(), "\"dummy\"");
}

TEST(Parser, ExprVar) {
  auto Src = R"(a)"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);
  ASSERT_EQ(AST->kind(), Node::NK_ExprVar);
  ASSERT_TRUE(AST->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().end().isAt(0, 1, 1));

  ASSERT_EQ(Diags.size(), 0);
}

TEST(Parser, SyncAttrs) {
  auto Src = R"(
rec {
  )))
  a asd =  1;
}
  )"sv;

  std::vector<Diagnostic> Diags;
  auto AST = nixf::parse(Src, Diags);

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 1);
  const auto &D = Diags[0];
  ASSERT_TRUE(D.range().begin().isAt(2, 2, 9));
  ASSERT_TRUE(D.range().end().isAt(3, 13, 26));
  ASSERT_EQ(D.kind(), Diagnostic::DK_UnexpectedText);
  ASSERT_EQ(D.args().size(), 0);

  // Check the note.
  ASSERT_EQ(D.notes().size(), 0);

  // Check fix-it hints.
  ASSERT_EQ(D.fixes().size(), 1);
  ASSERT_EQ(D.fixes()[0].edits().size(), 1);
  ASSERT_EQ(D.fixes()[0].message(), "remove unexpected text");
  const auto &F = D.fixes()[0].edits()[0];
  ASSERT_TRUE(F.oldRange().begin().isAt(2, 2, 9));
  ASSERT_TRUE(F.oldRange().end().isAt(3, 13, 26));
  ASSERT_EQ(F.newText(), "");
}

} // namespace
