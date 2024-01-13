#include <gtest/gtest.h>

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/DiagnosticEngine.h"
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
  ASSERT_EQ(Expr->range().view(), Src);
}

TEST(Parser, FloatLeading) {
  auto Src = "01.0"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprFloat);
  ASSERT_EQ(static_cast<ExprFloat *>(Expr.get())->value(), 1.0);
  ASSERT_EQ(Diags.diags().size(), 0); // FIXME! this should trigger diagnostic
  ASSERT_EQ(Expr->range().view(), Src);
}

TEST(Parser, FloatLeading00) {
  auto Src = "00.5"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprFloat);
  ASSERT_EQ(static_cast<ExprFloat *>(Expr.get())->value(), 0.5);
  ASSERT_EQ(Diags.diags().size(), 1);
  ASSERT_EQ(Expr->range().view(), Src);
}

TEST(Parser, StringSimple) {
  auto Src = R"("aaa")"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprString);
  auto Parts = static_cast<ExprString *>(Expr.get())->parts();
  ASSERT_EQ(Parts->range().view(), "aaa");
  ASSERT_EQ(Parts->fragments().size(), 1);
  ASSERT_EQ(Diags.diags().size(), 0);
  ASSERT_EQ(Expr->range().view(), Src);
}

TEST(Parser, StringMissingDQuote) {
  auto Src = R"("aaa)"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->kind(), Node::NK_ExprString);
  auto Parts = static_cast<ExprString *>(Expr.get())->parts();
  ASSERT_EQ(Parts->range().view(), "aaa");
  ASSERT_EQ(Parts->fragments().size(), 1);

  // Check the diagnostic.
  ASSERT_EQ(Diags.diags().size(), 1);
  auto &D = Diags.diags()[0];
  ASSERT_EQ(D->range().Begin, Src.begin() + 4);
  ASSERT_EQ(D->range().End, Src.begin() + 4);
  ASSERT_EQ(D->kind(), Diagnostic::DK_Expected);
  ASSERT_EQ(D->args().size(), 1);
  ASSERT_EQ(D->args()[0], "\" to close string literal");

  // Check the note.
  ASSERT_EQ(D->notes().size(), 1);
  auto &N = D->notes()[0];
  ASSERT_EQ(N->range().Begin, Src.begin() + 0);
  ASSERT_EQ(N->range().End, Src.begin() + 0);
  ASSERT_EQ(N->kind(), Note::NK_ToMachThis);
  ASSERT_EQ(N->args().size(), 1);
  ASSERT_EQ(N->args()[0], "\"");

  ASSERT_EQ(Expr->range().view(), Src);
}

} // namespace
