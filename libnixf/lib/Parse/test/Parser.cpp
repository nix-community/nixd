#include <gtest/gtest.h>

#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Parse/Nodes.h"
#include "nixf/Parse/Parser.h"

#include <string_view>

namespace {

using namespace nixf;

TEST(Parser, Integer) {
  using namespace std::literals;
  auto Src = "1"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->getKind(), Node::NK_ExprInt);
  ASSERT_EQ(static_cast<ExprInt *>(Expr.get())->getValue(), 1);
}

TEST(Parser, Float) {
  using namespace std::literals;
  auto Src = "1.0"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->getKind(), Node::NK_ExprFloat);
  ASSERT_EQ(static_cast<ExprFloat *>(Expr.get())->getValue(), 1.0);
  ASSERT_EQ(Diags.diags().size(), 0);
  ASSERT_EQ(Expr->getRange().view(), Src);
}

TEST(Parser, FloatLeading) {
  using namespace std::literals;
  auto Src = "01.0"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->getKind(), Node::NK_ExprFloat);
  ASSERT_EQ(static_cast<ExprFloat *>(Expr.get())->getValue(), 1.0);
  ASSERT_EQ(Diags.diags().size(), 0); // FIXME! this should trigger diagnostic
  ASSERT_EQ(Expr->getRange().view(), Src);
}

TEST(Parser, FloatLeading00) {
  using namespace std::literals;
  auto Src = "00.5"sv;
  DiagnosticEngine Diags;
  auto Expr = nixf::parse(Src, Diags);
  ASSERT_TRUE(Expr);
  ASSERT_EQ(Expr->getKind(), Node::NK_ExprFloat);
  ASSERT_EQ(static_cast<ExprFloat *>(Expr.get())->getValue(), 0.5);
  ASSERT_EQ(Diags.diags().size(), 1);
  ASSERT_EQ(Expr->getRange().view(), Src);
}

} // namespace
