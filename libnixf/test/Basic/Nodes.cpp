#include <gtest/gtest.h>

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes/Basic.h"
#include "nixf/Basic/Nodes/Simple.h"
#include "nixf/Parse/Parser.h"

namespace {

using namespace std::literals;
using namespace nixf;

TEST(Node, Descend) {
  auto Src = "{ a = 1; }"sv;
  std::vector<Diagnostic> Diag;
  auto Root = parse(Src, Diag);

  ASSERT_EQ(Root->descend({{0, 2}, {0, 2}})->kind(), Node::NK_Identifer);
  ASSERT_EQ(Root->descend({{0, 2}, {0, 4}})->kind(), Node::NK_Binding);
}

TEST(Node, InterpolateLiteral) {
  std::vector<InterpolablePart> Fragments;
  Fragments.emplace_back("foo");
  Fragments.emplace_back("bar");
  InterpolatedParts Parts(LexerCursorRange{}, std::move(Fragments));
  ASSERT_TRUE(Parts.isLiteral());
  ASSERT_EQ(Parts.literal(), "foobar");
}

TEST(Node, InterpolateLiteralFalse) {
  std::vector<InterpolablePart> Fragments;
  Fragments.emplace_back(
      std::make_unique<Interpolation>(LexerCursorRange{}, nullptr));
  InterpolatedParts Parts(LexerCursorRange{}, std::move(Fragments));
  ASSERT_FALSE(Parts.isLiteral());
}

TEST(Node, ExprCall_Children) {
  auto Src = "foo bar baz"sv;
  std::vector<Diagnostic> Diag;
  auto Root = parse(Src, Diag);

  ASSERT_EQ(Root->children().size(), 3);
}

TEST(Node, ExprString_Children) {
  auto Src = R"("foo${baz}")"sv;
  std::vector<Diagnostic> Diag;
  auto Root = parse(Src, Diag);

  ASSERT_EQ(Root->children().size(), 1);
  ASSERT_EQ(Root->children()[0]->children().size(), 1);
}

} // namespace
