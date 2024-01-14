#pragma once

#include "nixf/Basic/Range.h"

#include <cassert>
#include <string>

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
  RangeTy Range;

public:
  Token(tok::TokenKind Kind, RangeTy Range) : Kind(Kind), Range(Range) {}
  [[nodiscard]] Point begin() const { return Range.begin(); }
  [[nodiscard]] Point end() const { return Range.end(); }
  [[nodiscard]] tok::TokenKind kind() const { return Kind; }
  [[nodiscard]] RangeTy range() const { return Range; }
};

} // namespace nixf
