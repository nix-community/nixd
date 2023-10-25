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
    return Parser(*L, D);
  }
};

TEST_F(ParserTest, Int) {
  Parser P = get("1");
  std::shared_ptr<RawNode> R = P.parseExpr();
  auto *Tok = dynamic_cast<Token *>(R.get());
  ASSERT_EQ(R->getSyntaxKind(), SyntaxKind::SK_Token);
  ASSERT_EQ(Tok->getKind(), tok::tok_int);
}

} // namespace nixf
