#include <gtest/gtest.h>

#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Parse/Parser.h"
#include "nixf/Syntax/Syntax.h"

namespace nixf {

struct ParserTest : testing::Test {
  nixf::DiagnosticEngine D;
  std::unique_ptr<Lexer> L;
  Parser get(std::string_view Src) {
    L = std::make_unique<Lexer>(Src, D);
    return Parser(*L);
  }
};

TEST_F(ParserTest, Int) {
  nixf::Lexer L("1", D);
  nixf::Parser P(L);
  std::shared_ptr<ExprSyntax> R = P.parseSimple();
  ASSERT_EQ(R->Kind, ExprSyntax::EK_Int);
}

} // namespace nixf
