#pragma once

#include "nixf/Basic/Range.h"

#include <cassert>
#include <string>
#include <string_view>

namespace nixf {

namespace tok {
enum TokenKind {
#define TOK(NAME) tok_##NAME,
#include "Tokens.inc"
#undef TOK
};

constexpr std::string_view spelling(TokenKind Kind) {
  switch (Kind) {
#define TOK_KEYWORD(NAME)                                                      \
  case tok_kw_##NAME:                                                          \
    return #NAME;
#include "TokenKinds.inc"
#undef TOK_KEYWORD
  case tok_dquote:
    return "\"";
  case tok_quote2:
    return "''";
  case tok_dollar_curly:
    return "${";
  case tok_l_curly:
    return "{";
  case tok_r_curly:
    return "}";
  case tok_l_paren:
    return "(";
  case tok_r_paren:
    return ")";
  case tok_eq:
    return "=";
  case tok_semi_colon:
    return ";";
  case tok_l_bracket:
    return "[";
  case tok_r_bracket:
    return "]";
  default:
    assert(false && "Not yet implemented!");
  }
  __builtin_unreachable();
}

} // namespace tok

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
