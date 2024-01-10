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

class Token {
  tok::TokenKind Kind;
  OffsetRange Range;

public:
  Token(tok::TokenKind Kind, OffsetRange Range) : Kind(Kind), Range(Range) {}
  [[nodiscard]] const char *getBegin() const { return Range.Begin; }
  [[nodiscard]] const char *getEnd() const { return Range.End; }
  [[nodiscard]] tok::TokenKind getKind() const { return Kind; }
  [[nodiscard]] OffsetRange getRange() const { return Range; }
  [[nodiscard]] std::string_view view() const { return Range.view(); }
};

} // namespace nixf
