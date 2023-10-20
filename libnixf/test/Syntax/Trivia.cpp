#include <gtest/gtest.h>

#include "nixf/Syntax/Trivia.h"

namespace nixf {

TEST(Syntax, TriviaSpaces) {
  auto Piece = TriviaPiece::spaces(3);
  std::stringstream SS;
  SS << Piece;
  ASSERT_EQ(SS.str(), "   ");
}

TEST(Syntax, TriviaNewlines) {
  auto Piece = TriviaPiece::newlines(3);
  std::stringstream SS;
  SS << Piece;
  ASSERT_EQ(SS.str(), "\n\n\n");
}

} // namespace nixf
