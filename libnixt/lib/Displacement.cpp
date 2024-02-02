#include "nixt/Displacement.h"

namespace nixt {

nix::PosIdx displOf(const nix::Expr *E, nix::Displacement Displ) {
  if (const auto *CE = dynamic_cast<const nix::ExprAttrs *>(E))
    return displOf(CE, Displ);
  if (const auto *CE = dynamic_cast<const nix::ExprLet *>(E))
    return displOf(CE, Displ);
  if (const auto *CE = dynamic_cast<const nix::ExprLambda *>(E))
    return displOf(CE, Displ);

  assert(false && "The requested expr is not an env creator");
  return nix::noPos; // unreachable
}

nix::PosIdx displOf(const nix::ExprAttrs *E, nix::Displacement Displ) {
  assert(E->recursive && "Only recursive ExprAttr has displacement values");

  auto DefIt = E->attrs.begin();
  std::advance(DefIt, Displ);

  return DefIt->second.pos;
}

nix::PosIdx displOf(const nix::ExprLet *E, nix::Displacement Displ) {
  auto DefIt = E->attrs->attrs.begin();
  std::advance(DefIt, Displ);

  return DefIt->second.pos;
}

nix::PosIdx displOf(const nix::ExprLambda *E, nix::Displacement Displ) {
  if (E->arg) {
    if (Displ == 0)
      // It is just a symbol, so noPos.
      return nix::noPos;
    Displ--;
  }

  assert(E->hasFormals() && "Lambda must has formals to create displ");
  return E->formals->formals[Displ].pos;
}

} // namespace nixt
