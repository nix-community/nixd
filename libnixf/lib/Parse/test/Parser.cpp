#include <gtest/gtest.h>

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Basic/Range.h"
#include "nixf/Parse/Nodes.h"
#include "nixf/Parse/Parser.h"

#include <string_view>

namespace {

using namespace nixf;
using namespace std::literals;

TEST(Parser, Integer) {
  auto Src = "1"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprInt);
  ASSERT_EQ(static_cast<ExprInt *>(Expr.get())->value(), 1);
}

TEST(Parser, Float) {
  auto Src = "1.0"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprFloat);
  ASSERT_EQ(static_cast<ExprFloat *>(Expr.get())->value(), 1.0);
  ASSERT_EQ(Diags.diags().size(), 0);
  ASSERT_TRUE(Expr->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().end().isAt(0, 3, 3));
}

TEST(Parser, FloatLeading) {
  auto Src = "01.0"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprFloat);
  ASSERT_EQ(static_cast<ExprFloat *>(Expr.get())->value(), 1.0);

  // Check the diagnostic.
  ASSERT_EQ(Diags.diags().size(), 1);
  auto &D = Diags.diags()[0];
  ASSERT_TRUE(Expr->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().end().isAt(0, 4, 4));
  ASSERT_EQ(D->kind(), Diagnostic::DK_FloatLeadingZero);
  ASSERT_EQ(D->args().size(), 1);
  ASSERT_EQ(D->args()[0], "01");
}

TEST(Parser, FloatLeading00) {
  auto Src = "00.5"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprFloat);
  ASSERT_EQ(static_cast<ExprFloat *>(Expr.get())->value(), 0.5);
  ASSERT_EQ(Diags.diags().size(), 1);
  ASSERT_TRUE(Expr->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().end().isAt(0, 4, 4));
}

TEST(Parser, StringSimple) {
  auto Src = R"("aaa")"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprString);
  auto Parts = static_cast<ExprString *>(Expr.get())->parts();
  ASSERT_TRUE(Parts->range().begin().isAt(0, 1, 1));
  ASSERT_TRUE(Parts->range().end().isAt(0, 4, 4));
  ASSERT_EQ(Parts->fragments().size(), 1);
  ASSERT_EQ(Diags.diags().size(), 0);
  ASSERT_TRUE(Expr->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().end().isAt(0, 5, 5));
}

TEST(Parser, StringMissingDQuote) {
  auto Src = R"("aaa)"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprString);
  auto Parts = static_cast<ExprString *>(Expr.get())->parts();
  ASSERT_TRUE(Parts->range().begin().isAt(0, 1, 1));
  ASSERT_TRUE(Parts->range().end().isAt(0, 4, 4));
  ASSERT_EQ(Parts->fragments().size(), 1);

  // Check the diagnostic.
  ASSERT_EQ(Diags.diags().size(), 1);
  auto &D = Diags.diags()[0];
  ASSERT_TRUE(Expr->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().end().isAt(0, 4, 4));
  ASSERT_EQ(D->kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D->args().size(), 1);
  ASSERT_EQ(D->args()[0], "\"");

  // Check the note.
  ASSERT_EQ(D->notes().size(), 1);
  auto &N = D->notes()[0];
  ASSERT_TRUE(N->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(N->range().end().isAt(0, 1, 1));
  ASSERT_EQ(N->kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N->args().size(), 1);
  ASSERT_EQ(N->args()[0], "\"");

  // Check fix-it hints.
  ASSERT_EQ(D->fixes().size(), 1);
  const auto &F = D->fixes()[0];
  ASSERT_TRUE(F.oldRange().begin().isAt(0, 4, 4));
  ASSERT_TRUE(F.oldRange().end().isAt(0, 4, 4));
  ASSERT_EQ(F.newText(), "\"");
}

TEST(Parser, StringInterpolation) {
  auto Src = R"("aaa ${1} bbb")"sv;
  DiagnosticEngine Diags;
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

  ASSERT_EQ(Diags.diags().size(), 0);
  ASSERT_TRUE(Expr->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(Expr->range().end().isAt(0, 14, 14));
}

TEST(Parser, IndentedString) {
  auto Src = R"(''aaa
  foo

  bar

  ${''string''}

  )"sv;

  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprString);
  auto Parts = static_cast<ExprString *>(Expr.get())->parts();
  ASSERT_EQ(Parts->fragments().size(), 3);

  // Check the diagnostic.
  ASSERT_EQ(Diags.diags().size(), 1);
  auto &D = Diags.diags()[0];
  ASSERT_TRUE(D->range().begin().isAt(7, 3, 39));
  ASSERT_TRUE(D->range().end().isAt(7, 3, 39));
  ASSERT_EQ(D->kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D->args().size(), 1);
  ASSERT_EQ(D->args()[0], "''");

  // Check the note.
  ASSERT_EQ(D->notes().size(), 1);
  auto &N = D->notes()[0];
  ASSERT_TRUE(N->range().begin().isAt(0, 0, 0));
  ASSERT_TRUE(N->range().end().isAt(0, 2, 2));
  ASSERT_EQ(N->kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N->args().size(), 1);
  ASSERT_EQ(N->args()[0], "''");

  // Check fix-it hints.
  ASSERT_EQ(D->fixes().size(), 1);
  const auto &F = D->fixes()[0];
  ASSERT_TRUE(F.oldRange().begin().isAt(7, 3, 39));
  ASSERT_TRUE(F.oldRange().end().isAt(7, 3, 39));
  ASSERT_EQ(F.newText(), "''");
}

} // namespace
