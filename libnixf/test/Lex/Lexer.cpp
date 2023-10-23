#include "nixf/Lex/Lexer.h"
#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Syntax/Token.h"
#include "nixf/Syntax/Trivia.h"
#include <gtest/gtest.h>

namespace nixf {

using namespace tok;
struct LexerTest : testing::Test {
  DiagnosticEngine Diag;
  std::stringstream SS;
};

TEST_F(LexerTest, Integer) {
  Lexer Lexer("1", Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->Kind, tok_int);
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, Integer2) {
  Lexer Lexer("1123123", Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->Kind, tok_int);
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, Integer4) {
  Lexer Lexer("00023121123123", Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->Kind, tok_int);
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, Integer5) {
  Lexer Lexer("00023121123123", Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->Kind, tok_int);
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, Trivia1) {
  std::string Trivia("\r\n /* */# line comment\n\f \v\r \n");
  std::string Src = Trivia + "3";
  Lexer Lexer(Src, Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->Kind, tok_int);
  ASSERT_EQ(P->LeadingTrivia.Pieces.size(), 11);
  SS << P->LeadingTrivia;
  ASSERT_EQ(SS.str(), Trivia);
  ASSERT_EQ(P->Content, "3");
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, TriviaLComment) {
  Lexer Lexer(R"(# single line comment

3
)",
              Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->Kind, tok_int);
  ASSERT_EQ(P->LeadingTrivia.Pieces.size(), 2);
  SS << P->LeadingTrivia;
  ASSERT_EQ(SS.str(), "# single line comment\n\n");
  ASSERT_EQ(P->Content, "3");
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, TriviaBComment) {
  const char *Src = R"(/* block comment
aaa
*/)";
  Lexer Lexer(Src, Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->Kind, tok_eof);
  ASSERT_EQ(P->LeadingTrivia.Pieces.size(), 1);
  SS << P->LeadingTrivia;
  ASSERT_EQ(SS.str(), "/* block comment\naaa\n*/");
  ASSERT_EQ(P->Content, "");
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, TriviaBComment2) {
  const char *Src = R"(/* block comment
aaa
)";
  Lexer Lexer(Src, Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->Kind, tok_eof);
  ASSERT_EQ(P->LeadingTrivia.Pieces.size(), 1);
  ASSERT_EQ(P->LeadingTrivia.Pieces[0].Kind, TriviaKind::BlockComment);
  ASSERT_EQ(P->LeadingTrivia.Pieces[0].Count, 1);
  ASSERT_EQ(P->LeadingTrivia.Pieces[0].Text, Src);
  ASSERT_EQ(P->Content, "");
  ASSERT_TRUE(!Diag.diags().empty());

  ASSERT_EQ(std::string(Diag.diags()[0]->format()), "unterminated /* comment");
  ASSERT_EQ(std::string(Diag.diags()[0]->notes()[0]->format()),
            "/* comment begins at here");
}

TEST_F(LexerTest, FloatLeadingZero) {
  Lexer Lexer("00.33", Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->Kind, tok_float);
  ASSERT_EQ(P->Content, "00.33");
  ASSERT_FALSE(Diag.diags().empty());
  ASSERT_EQ(std::string(Diag.diags()[0]->format()),
            "float begins with extra zeros `00.33` is nixf extension");
}

TEST_F(LexerTest, FloatNoExp) {
  Lexer Lexer("00.33e", Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->Kind, tok_err);
  ASSERT_EQ(P->Content, "00.33e");
  ASSERT_FALSE(Diag.diags().empty());
  ASSERT_EQ(std::string(Diag.diags()[0]->format()),
            "float point has trailing `e` but has no exponential part");
}

} // namespace nixf
