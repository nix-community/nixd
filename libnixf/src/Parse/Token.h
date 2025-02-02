#pragma once

#include "Tokens.h"
#include "nixf/Basic/Range.h"

#include <cassert>
#include <string_view>

namespace nixf {

/// \brief A token. With it's kind, and the range in source code.
///
/// This class is trivially copyable.
class Token {
  tok::TokenKind Kind;
  LexerCursorRange Range;
  std::string_view View;

public:
  Token(tok::TokenKind Kind, LexerCursorRange Range, std::string_view View)
      : Kind(Kind), Range(Range), View(View) {}
  [[nodiscard]] LexerCursor lCur() const { return Range.lCur(); }
  [[nodiscard]] LexerCursor rCur() const { return Range.rCur(); }
  [[nodiscard]] tok::TokenKind kind() const { return Kind; }
  [[nodiscard]] LexerCursorRange range() const { return Range; }
  [[nodiscard]] std::string_view view() const { return View; }
};

} // namespace nixf
