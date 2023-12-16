#include "nixt/Name.h"

namespace nixt {

const char *nameOf(const nix::Expr *E) {
#define NIX_EXPR(EXPR)                                                         \
  if (dynamic_cast<const nix::EXPR *>(E)) {                                    \
    return #EXPR;                                                              \
  }
#include "nixt/Nodes.inc"
  assert(false &&
         "Cannot dynamic-cast to nix::Expr*, missing entries in Nodes.inc?");
  return nullptr;
#undef NIX_EXPR
}

} // namespace nixt
