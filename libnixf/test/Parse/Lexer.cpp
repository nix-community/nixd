#include <gtest/gtest.h>

#include "Lexer.h"

#include "nixf/Basic/Diagnostic.h"

#include <cstddef>

namespace {

using namespace nixf;
using namespace tok;

auto collect(Lexer &L, Token (Lexer::*Ptr)()) {
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
  std::vector<Diagnostic> Diags;
  std::stringstream SS;
};

TEST_F(LexerTest, Integer) {
  Lexer Lexer("1", Diags);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_int);
  ASSERT_TRUE(Diags.empty());
}

TEST_F(LexerTest, Integer2) {
  Lexer Lexer("1123123", Diags);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_int);
  ASSERT_TRUE(Diags.empty());
}

TEST_F(LexerTest, Integer4) {
  Lexer Lexer("00023121123123", Diags);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_int);
  ASSERT_TRUE(Diags.empty());
}

TEST_F(LexerTest, Integer5) {
  Lexer Lexer("00023121123123", Diags);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_int);
  ASSERT_TRUE(Diags.empty());
}

TEST_F(LexerTest, Trivia1) {
  std::string Trivia("\r\n /* */# line comment\n\f \v\r \n");
  std::string Src = Trivia + "3";
  Lexer Lexer(Src, Diags);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_int);
  ASSERT_EQ(P.view(), "3");
  ASSERT_TRUE(Diags.empty());
}

TEST_F(LexerTest, TriviaLComment) {
  Lexer Lexer(R"(# single line comment

3
)",
              Diags);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_int);
  ASSERT_EQ(P.view(), "3");
  ASSERT_TRUE(Diags.empty());
}

TEST_F(LexerTest, TriviaBComment) {
  const char *Src = R"(/* block comment
aaa
*/)";
  Lexer Lexer(Src, Diags);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_eof);
  ASSERT_EQ(P.view(), "");
  ASSERT_TRUE(Diags.empty());
}

TEST_F(LexerTest, TriviaBComment2) {
  const char *Src = R"(/* block comment
aaa
)";
  Lexer Lexer(Src, Diags);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_eof);
  ASSERT_EQ(P.view(), "");
  ASSERT_TRUE(!Diags.empty());

  ASSERT_EQ(std::string(Diags[0].message()), "unterminated /* comment");
  ASSERT_EQ(std::string(Diags[0].notes()[0].message()),
            "/* comment begins at here");
}

TEST_F(LexerTest, FloatLeadingZero) {
  Lexer Lexer("00.33", Diags);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_float);
  ASSERT_EQ(P.view(), "00.33");
  ASSERT_FALSE(Diags.empty());
  ASSERT_EQ(std::string(Diags[0].message()),
            "float begins with extra zeros `{}` is nixf extension");
  ASSERT_EQ(std::string(Diags[0].args()[0]), "00");
}

TEST_F(LexerTest, FloatNoExp_little) {
  Lexer Lexer("0.33e", Diags);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_float);
  ASSERT_EQ(P.view(), "0.33e");
  ASSERT_FALSE(Diags.empty());
  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_FloatNoExp);
  ASSERT_EQ(std::string(Diags[0].args()[0]), "e");
}

TEST_F(LexerTest, FloatNoExp_big) {
  Lexer Lexer("0.33E", Diags);
  auto P = Lexer.lex();
  ASSERT_EQ(P.kind(), tok_float);
  ASSERT_EQ(P.view(), "0.33E");
  ASSERT_FALSE(Diags.empty());
  ASSERT_EQ(Diags[0].kind(), Diagnostic::DK_FloatNoExp);
  ASSERT_EQ(std::string(Diags[0].args()[0]), "E");
}

TEST_F(LexerTest, lexString) {
  Lexer Lexer(R"("aa bb \\ \t \" \n ${}")", Diags);
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
  Lexer Lexer(R"(id pa/t)", Diags);
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
  Lexer Lexer(R"(if then)", Diags);
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
  Lexer Lexer(R"(https://github.com/inclyc/libnixf)", Diags);
  auto Tokens = collect(Lexer, &Lexer::lex);
  const TokenKind Match[] = {tok_uri};
  for (size_t I = 0; I < sizeof(Match) / sizeof(TokenKind); I++) {
    ASSERT_EQ(Tokens[I].kind(), Match[I]);
  }
  ASSERT_EQ(Tokens.size(), sizeof(Match) / sizeof(TokenKind));
}

TEST_F(LexerTest, IndStringEscape) {
  Lexer Lexer(R"(''\$${)", Diags);
  const TokenKind Match[] = {
      tok_string_escape, // ''\ escapes $
      tok_dollar_curly,  // ${
  };
  auto Tokens = collect(Lexer, &Lexer::lexIndString);
  for (size_t I = 0; I < Tokens.size(); I++) {
    ASSERT_EQ(Tokens[I].kind(), Match[I]);
  }
  ASSERT_EQ(Tokens.size(), sizeof(Match) / sizeof(TokenKind));
}

} // namespace
