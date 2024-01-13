#include <gtest/gtest.h>

#include "Lexer.h"

#include "nixf/Basic/DiagnosticEngine.h"

#include <cstddef>

namespace nixf {

using namespace tok;

static auto collect(Lexer &L, Token (Lexer::*Ptr)()) {
  std::vector<Token> Ret;
  while (true) {
    Token Tok = (L.*Ptr)();
    if (Tok.kind() == tok_eof)
      break;
    Ret.emplace_back(Tok);
  }
  return Ret;
}

struct LexerTest : testing::Test {
  DiagnosticEngine Diag;
  std::stringstream SS;
};

TEST_F(LexerTest, Integer) {
  Lexer Lexer("1", Diag);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_int);
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, Integer2) {
  Lexer Lexer("1123123", Diag);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_int);
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, Integer4) {
  Lexer Lexer("00023121123123", Diag);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_int);
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, Integer5) {
  Lexer Lexer("00023121123123", Diag);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_int);
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, Trivia1) {
  std::string Trivia("\r\n /* */# line comment\n\f \v\r \n");
  std::string Src = Trivia + "3";
  Lexer Lexer(Src, Diag);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_int);
  ASSERT_EQ(P.view(), "3");
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, TriviaLComment) {
  Lexer Lexer(R"(# single line comment

3
)",
              Diag);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_int);
  ASSERT_EQ(P.view(), "3");
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, TriviaBComment) {
  const char *Src = R"(/* block comment
aaa
*/)";
  Lexer Lexer(Src, Diag);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_eof);
  ASSERT_EQ(P.view(), "");
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, TriviaBComment2) {
  const char *Src = R"(/* block comment
aaa
)";
  Lexer Lexer(Src, Diag);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_eof);
  ASSERT_EQ(P.view(), "");
  ASSERT_TRUE(!Diag.diags().empty());

  ASSERT_EQ(std::string(Diag.diags()[0]->message()), "unterminated /* comment");
  ASSERT_EQ(std::string(Diag.diags()[0]->notes()[0]->message()),
            "/* comment begins at here");
}

TEST_F(LexerTest, FloatLeadingZero) {
  Lexer Lexer("00.33", Diag);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_float);
  ASSERT_EQ(P.view(), "00.33");
  ASSERT_FALSE(Diag.diags().empty());
  ASSERT_EQ(std::string(Diag.diags()[0]->message()),
            "float begins with extra zeros `{}` is nixf extension");
  ASSERT_EQ(std::string(Diag.diags()[0]->args()[0]), "00.33");
}

TEST_F(LexerTest, lexString) {
  Lexer Lexer(R"("aa bb \\ \t \" \n ${}")", Diag);
  const TokenKind Match[] = {
      tok_dquote,        // '"'
      tok_string_part,   // 'aa bb '
      tok_string_escape, // '\\'
      tok_string_part,   // ' '
      tok_string_escape, // '\t'
      tok_string_part,   // ' '
      tok_string_escape, // '\"'
      tok_string_part,   // ' '
      tok_string_escape, // '\n'
      tok_string_part,   // ' '
      tok_dollar_curly,  // '${'
      tok_string_part,   // '}'
      tok_dquote,        // '"'
  };
  auto Tokens = collect(Lexer, &Lexer::lexString);
  for (size_t I = 0; I < Tokens.size(); I++) {
    ASSERT_EQ(Tokens[I].kind(), Match[I]);
  }
  ASSERT_EQ(Tokens.size(), 13);
}

TEST_F(LexerTest, lexIDPath) {
  // FIXME: test  pp//a to see that we can lex this as Update(pp, a)
  Lexer Lexer(R"(id pa/t)", Diag);
  const TokenKind Match[] = {
      tok_id,            // id
      tok_path_fragment, // pa/
      tok_id,            // t
  };
  auto Tokens = collect(Lexer, &Lexer::lex);
  for (size_t I = 0; I < sizeof(Match) / sizeof(TokenKind); I++) {
    ASSERT_EQ(Tokens[I].kind(), Match[I]);
  }
  ASSERT_EQ(Tokens.size(), sizeof(Match) / sizeof(TokenKind));
}

TEST_F(LexerTest, lexKW) {
  // FIXME: test  pp//a to see that we can lex this as Update(pp, a)
  Lexer Lexer(R"(if then)", Diag);
  const TokenKind Match[] = {
      tok_kw_if,   // if
      tok_kw_then, // then
  };
  auto Tokens = collect(Lexer, &Lexer::lex);
  for (size_t I = 0; I < sizeof(Match) / sizeof(TokenKind); I++) {
    ASSERT_EQ(Tokens[I].kind(), Match[I]);
  }
  ASSERT_EQ(Tokens.size(), sizeof(Match) / sizeof(TokenKind));
}

TEST_F(LexerTest, lexURI) {
  Lexer Lexer(R"(https://github.com/inclyc/libnixf)", Diag);
  auto Tokens = collect(Lexer, &Lexer::lex);
  const TokenKind Match[] = {tok_uri};
  for (size_t I = 0; I < sizeof(Match) / sizeof(TokenKind); I++) {
    ASSERT_EQ(Tokens[I].kind(), Match[I]);
  }
  ASSERT_EQ(Tokens.size(), sizeof(Match) / sizeof(TokenKind));
}

} // namespace nixf
