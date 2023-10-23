#include "nixf/Syntax/Syntax.h"

namespace nixf {

const char *Syntax::getName(SyntaxKind Kind) {
  switch (Kind) {
#define EXPR(NAME)                                                             \
  case SK_##NAME:                                                              \
    return #NAME;
#include "nixf/Syntax/SyntaxKinds.inc"
#undef EXPR
  default:
    __builtin_unreachable();
  }
}
} // namespace nixf
