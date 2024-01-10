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

} // namespace
