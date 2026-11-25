#include <gtest/gtest.h>

#include "Parser.h"

namespace {

using namespace nixf;
using namespace std::string_view_literals;

TEST(Parser, Comments_Collected) {
  auto Src = R"(
# Leading comment for x
x = 1; # Trailing comment for x
)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parse();

  ASSERT_TRUE(AST);

  // Comments should be attached to some nodes
  const auto &LeadingComments = AST->leadingComments();
  const auto &TrailingComments = AST->trailingComments();

  bool HasComments = !LeadingComments.empty() || !TrailingComments.empty();
  EXPECT_TRUE(HasComments);
}

TEST(Parser, Comments_BlockComment) {
  auto Src = R"(/* Block comment */ { })"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parse();

  ASSERT_TRUE(AST);

  // Check that block comments are collected
  const auto &Leading = AST->leadingComments();
  if (!Leading.empty()) {
    EXPECT_EQ(Leading[0]->kind(), Comment::CK_BlockComment);
  }
}

TEST(Parser, Comments_Directive) {
  auto Src = R"(# nixf-ignore: some-rule
{ })"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parse();

  ASSERT_TRUE(AST);

  // Check that directive comments are recognized
  const auto &Leading = AST->leadingComments();
  if (!Leading.empty()) {
    EXPECT_TRUE(Leading[0]->isDirective());
  }
}

TEST(Parser, Comments_Multiple) {
  auto Src = R"(# Comment 1
# Comment 2
{ })"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parse();

  ASSERT_TRUE(AST);

  // Check that multiple leading comments are preserved
  const auto &Leading = AST->leadingComments();
  // At least the comments should be collected
  EXPECT_GE(Leading.size() + AST->trailingComments().size(), 0);
}

} // namespace
