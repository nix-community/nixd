#include "nixt/Kinds.h"

#include <nix/expr/nixexpr.hh>

namespace nixt {

using namespace ek;

ExprKind kindOf(const nix::Expr &E) {
  const nix::Expr *P = &E;
#define NIX_EXPR(EXPR)                                                         \
  if (dynamic_cast<const nix::EXPR *>(P)) {                                    \
    return EK_##EXPR;                                                          \
  }
#include "nixt/Nodes.inc"
  assert(false &&
         "Cannot dynamic-cast to nix::Expr*, missing entries in Nodes.inc?");
  return LastExprKind;
#undef NIX_EXPR
}

const char *nameOf(ExprKind Kind) {
  switch (Kind) {
#define NIX_EXPR(EXPR)                                                         \
  case EK_##EXPR:                                                              \
    return #EXPR;
#include "nixt/Nodes.inc"
#undef NIX_EXPR
  case LastExprKind:
    break;
  }
  assert(false && "Unknown ExprKind");
  return "<unknown>";
}

} // namespace nixt
