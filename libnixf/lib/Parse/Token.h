#pragma once

#include "nixf/Basic/Range.h"
#include <string>

namespace nixf {

namespace tok {
enum TokenKind {
#define TOK(NAME) tok_##NAME,
#include "Tokens.inc"
#undef TOK
};

} // namespace tok

/// \brief A token. With it's kind, and the range in source code.
///
/// This class is trivially copyable.
class Token {
  tok::TokenKind Kind;
  OffsetRange Range;

public:
  Token(tok::TokenKind Kind, OffsetRange Range) : Kind(Kind), Range(Range) {}
  [[nodiscard]] const char *begin() const { return Range.Begin; }
  [[nodiscard]] const char *end() const { return Range.End; }
  [[nodiscard]] tok::TokenKind kind() const { return Kind; }
  [[nodiscard]] OffsetRange range() const { return Range; }
  [[nodiscard]] std::string_view view() const { return Range.view(); }
};

} // namespace nixf
