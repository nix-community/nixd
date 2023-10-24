#include <gtest/gtest.h>

#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Parse/Parser.h"
#include "nixf/Syntax/Syntax.h"
#include "nixf/Syntax/Token.h"

namespace nixf {

struct ParserTest : testing::Test {
  nixf::DiagnosticEngine D;
  std::stringstream SS;
  std::unique_ptr<Lexer> L;
  Parser get(std::string_view Src) {
    L = std::make_unique<Lexer>(Src, D);
    return Parser(*L);
  }
};

TEST_F(ParserTest, Int) {
  nixf::Lexer L("1", D);
  nixf::Parser P(L);
  std::shared_ptr<RawNode> R = P.parseExpr();
  auto *Tok = dynamic_cast<Token *>(R.get());
  ASSERT_EQ(R->getSyntaxKind(), SyntaxKind::SK_Token);
  ASSERT_EQ(Tok->getKind(), tok::tok_int);
}

TEST_F(ParserTest, String) {
  nixf::Lexer L(R"("aaaa")", D);
  nixf::Parser P(L);
  std::shared_ptr<RawNode> R = P.parseExpr();
  R->dumpAST(SS);
  ASSERT_EQ(SS.str(), " ");
  ASSERT_EQ(R->getSyntaxKind(), SyntaxKind::SK_String);
}

} // namespace nixf
