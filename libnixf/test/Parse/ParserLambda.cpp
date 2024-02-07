#include <gtest/gtest.h>

#include "Parser.h"

namespace {

using namespace nixf;
using namespace std::string_view_literals;

TEST(Parser, ParseFormal_ID) {
  auto Src = R"(a)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);

  auto AST = P.parseFormal();

  ASSERT_TRUE(AST);
  ASSERT_EQ(Diags.size(), 0);

  ASSERT_EQ(AST->kind(), Node::NK_Formal);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 1, 1));
}

TEST(Parser, ParseFormal_CommaID) {
  auto Src = R"(, a)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);

  auto AST = P.parseFormal();

  ASSERT_TRUE(AST);
  ASSERT_EQ(Diags.size(), 0);

  ASSERT_EQ(AST->kind(), Node::NK_Formal);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 3, 3));
}

TEST(Parser, ParseFormal_ExprMissing) {
  auto Src = R"(, a ? )"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);

  auto AST = P.parseFormal();

  ASSERT_TRUE(AST);
  ASSERT_EQ(Diags.size(), 1);

  ASSERT_EQ(AST->kind(), Node::NK_Formal);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 5, 5));
}

TEST(Parser, ParseFormal_ExprOK) {
  auto Src = R"(, a ? 1)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);

  auto AST = P.parseFormal();

  ASSERT_TRUE(AST);
  ASSERT_EQ(Diags.size(), 0);

  ASSERT_EQ(AST->kind(), Node::NK_Formal);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 7, 7));
}

TEST(Parser, ParseFormal_Ellipsis) {
  auto Src = R"(, ...)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);

  auto AST = P.parseFormal();

  ASSERT_TRUE(AST);
  ASSERT_EQ(Diags.size(), 0);

  ASSERT_EQ(AST->kind(), Node::NK_Formal);
  ASSERT_TRUE(AST->range().lCur().isAt(0, 0, 0));
  ASSERT_TRUE(AST->range().rCur().isAt(0, 5, 5));
}

TEST(Parser, ParseFormals) {
  auto Src = R"({ a, b, c })"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);

  auto AST = P.parseFormals();

  ASSERT_TRUE(AST);
  ASSERT_EQ(Diags.size(), 0);

  ASSERT_EQ(AST->kind(), Node::NK_Formals);

  auto *F = static_cast<Formals *>(AST.get());
  ASSERT_EQ(F->members().size(), 3);
  ASSERT_TRUE(F->range().rCur().isAt(0, 11, 11));
}

TEST(Parser, ParseFormals_Recover) {
  auto Src = R"({ a, b, c ${ , d })"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);

  auto AST = P.parseFormals();

  ASSERT_TRUE(AST);

  ASSERT_EQ(Diags.size(), 1);

  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_UnexpectedText);
  ASSERT_TRUE(Diags[0].range().lCur().isAt(0, 10, 10));
  ASSERT_TRUE(Diags[0].range().rCur().isAt(0, 12, 12));
}

TEST(Parser, ParseLambdaArg_Formal) {
  auto Src = R"({ a, b, ... })"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseLambdaArg();

  ASSERT_TRUE(AST);
  ASSERT_EQ(Diags.size(), 0);
}

TEST(Parser, ParseLambdaArgID_Suffix) {
  auto Src = R"({ a, b, ... } @ ID)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseLambdaArg();

  ASSERT_TRUE(AST);
  ASSERT_EQ(Diags.size(), 0);
}

TEST(Parser, ParseLambdaArgID_Prefix) {
  auto Src = R"(ID @ { a, b, ... })"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseLambdaArg();

  ASSERT_TRUE(AST);
  ASSERT_EQ(Diags.size(), 0);
}

TEST(Parser, ParseLambdaArg_ID) {
  auto Src = R"(ID)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseLambdaArg();

  ASSERT_TRUE(AST);
  ASSERT_EQ(Diags.size(), 0);
}

TEST(Parser, ParseExprLambda) {
  auto Src = R"({ a, b, ... }: a)"sv;

  std::vector<Diagnostic> Diags;
  Parser P(Src, Diags);
  auto AST = P.parseExprLambda();

  ASSERT_TRUE(AST);
  ASSERT_EQ(Diags.size(), 0);
}

} // namespace
