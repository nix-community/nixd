#include <gtest/gtest.h>

#include "nixf/Syntax/Syntax.h"
#include "nixf/Syntax/SyntaxData.h"
#include "nixf/Syntax/Token.h"

namespace nixf {

TEST(Syntax, FormalPrint) {
  auto Tok = std::make_shared<Token>(tok::tok_int, "123", nullptr, nullptr);
  auto Data = std::make_shared<SyntaxData>(0, Tok);
  FormalSyntax Syntax(Data, Data.get());
  std::stringstream SS;
  Syntax.getRaw()->dump(SS);
  ASSERT_EQ(SS.str(), "123");
}

} // namespace nixf
