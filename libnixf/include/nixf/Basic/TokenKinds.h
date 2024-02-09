#pragma once

namespace nixf::tok {

enum TokenKind {
#define TOK(NAME) tok_##NAME,
#include "nixf/Basic/Tokens.inc"
#undef TOK
};

} // namespace nixf::tok
