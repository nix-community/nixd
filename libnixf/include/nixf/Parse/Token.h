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

struct Token {
  tok::TokenKind Kind;
  OffsetRange Range;
};

} // namespace nixf
