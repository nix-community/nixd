#include <gtest/gtest.h>

#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Lex/Lexer.h"
#include "nixf/Syntax/Token.h"
#include "nixf/Syntax/Trivia.h"

#include <cstddef>

namespace nixf {

using namespace tok;

static auto collect(Lexer &L, TokenView (Lexer::*Ptr)()) {
  std::vector<TokenView> Ret;
  while (true) {
    TokenView Tok = (L.*Ptr)();
    if (Tok->getKind() == tok_eof)
      break;
    Ret.emplace_back(std::move(Tok));
  }
  return Ret;
}

struct LexerTest : testing::Test {
  DiagnosticEngine Diag;
  std::stringstream SS;
};

TEST_F(LexerTest, Integer) {
  Lexer Lexer("1", Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->getKind(), tok_int);
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, Integer2) {
  Lexer Lexer("1123123", Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->getKind(), tok_int);
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, Integer4) {
  Lexer Lexer("00023121123123", Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->getKind(), tok_int);
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, Integer5) {
  Lexer Lexer("00023121123123", Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->getKind(), tok_int);
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, Trivia1) {
  std::string Trivia("\r\n /* */# line comment\n\f \v\r \n");
  std::string Src = Trivia + "3";
  Lexer Lexer(Src, Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->getKind(), tok_int);
  P->getLeadingTrivia()->dump(SS, /*DiscardTrivia=*/false);
  ASSERT_EQ(SS.str(), Trivia);
  ASSERT_EQ(P->getContent(), "3");
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, TriviaLComment) {
  Lexer Lexer(R"(# single line comment

3
)",
              Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->getKind(), tok_int);
  P->getLeadingTrivia()->dump(SS, /*DiscardTrivia=*/false);
  ASSERT_EQ(SS.str(), "# single line comment\n\n");
  ASSERT_EQ(P->getContent(), "3");
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, TriviaBComment) {
  const char *Src = R"(/* block comment
aaa
*/)";
  Lexer Lexer(Src, Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->getKind(), tok_eof);
  P->getLeadingTrivia()->dump(SS, /*DiscardTrivia=*/false);
  ASSERT_EQ(SS.str(), "/* block comment\naaa\n*/");
  ASSERT_EQ(P->getContent(), "");
  ASSERT_TRUE(Diag.diags().empty());
}

TEST_F(LexerTest, TriviaBComment2) {
  const char *Src = R"(/* block comment
aaa
)";
  Lexer Lexer(Src, Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->getKind(), tok_eof);
  ASSERT_EQ(P->getContent(), "");
  ASSERT_TRUE(!Diag.diags().empty());

  ASSERT_EQ(std::string(Diag.diags()[0]->format()), "unterminated /* comment");
  ASSERT_EQ(std::string(Diag.diags()[0]->notes()[0]->format()),
            "/* comment begins at here");
}

TEST_F(LexerTest, FloatLeadingZero) {
  Lexer Lexer("00.33", Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->getKind(), tok_float);
  ASSERT_EQ(P->getContent(), "00.33");
  ASSERT_FALSE(Diag.diags().empty());
  ASSERT_EQ(std::string(Diag.diags()[0]->format()),
            "float begins with extra zeros `00.33` is nixf extension");
}

TEST_F(LexerTest, FloatNoExp) {
  Lexer Lexer("00.33e", Diag);
  std::shared_ptr<Token> P = Lexer.lex();
  ASSERT_EQ(P->getKind(), tok_err);
  ASSERT_EQ(P->getContent(), "00.33e");
  ASSERT_FALSE(Diag.diags().empty());
  ASSERT_EQ(std::string(Diag.diags()[0]->format()),
            "float point has trailing `e` but has no exponential part");
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
    ASSERT_EQ(Tokens[I]->getKind(), Match[I]);
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
    ASSERT_EQ(Tokens[I]->getKind(), Match[I]);
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
    ASSERT_EQ(Tokens[I]->getKind(), Match[I]);
  }
  ASSERT_EQ(Tokens.size(), sizeof(Match) / sizeof(TokenKind));
}

} // namespace nixf
