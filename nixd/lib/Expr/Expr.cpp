#include "nixd/Expr/Expr.h"

#include <nixt/Displacement.h>
#include <nixt/Visitor.h>

#include <nix/nixexpr.hh>

#include <algorithm>
#include <iterator>

using namespace nixt;

namespace nixd {

bool isEnvCreated(const nix::Expr *E, const nix::Expr *Child) {

  // Line No. in the comment here is on NixOS/nix revision:
  //
  //   4539ab530ad23a8558512f784bd72c4cd0e72f13
  //

  // Custom, we need to check the relationship between parent & child
  // ----------------------------------------------------------------

  // src/libexpr/nixexpr.cc:507
  if (const auto *CE = dynamic_cast<const nix::ExprAttrs *>(E))
    return isEnvCreated(CE, Child);

  // src/libexpr/nixexpr.cc:507
  if (const auto *CE = dynamic_cast<const nix::ExprWith *>(E))
    return isEnvCreated(CE, Child);

  // Always true, these `Expr`s always created an Env.
  // -------------------------------------------------

  // src/libexpr/nixexpr.cc:435
  if (dynamic_cast<const nix::ExprLambda *>(E))
    return true;

  // src/libexpr/nixexpr.cc:472
  if (dynamic_cast<const nix::ExprLet *>(E))
    return true;

  return false; // Most `nix::Expr`s do not create a new Env.
}

bool isEnvCreated(const nix::ExprAttrs *E, const nix::Expr *) {
  return E->recursive;
}

bool isEnvCreated(const nix::ExprWith *E, const nix::Expr *Child) {
  // ExprWith creates a new env for it's body, not for it's "attrs"
  return E->body == Child;
}

const nix::Expr *
searchEnvExpr(const nix::ExprVar *E,
              const std::map<const nix::Expr *, const nix::Expr *> &ParentMap) {

  assert(!E->fromWith && "This expression binds to 'with' expression!");

  // For expressions not obtained from 'with' expression
  // nix get it's value as:

  //    for (auto l = var.level; l; --l, env = env->up) // Search "levels" up
  //    Value = env->values[var.displ];

  const nix::Expr *EnvExpr = E;
  for (auto Level = E->level + 1; Level;) {
    // So which expression is associated with 'Env->up'?
    // Should be a parent nix::Expr that creates env.
    const auto *ParentExpr = ParentMap.at(EnvExpr);

    // Might be "builtin" variable
    if (ParentExpr == EnvExpr)
      return nullptr;

    if (isEnvCreated(ParentExpr, EnvExpr))
      Level--;

    EnvExpr = ParentExpr;
  }

  return EnvExpr;
}

nix::PosIdx searchDefinition(
    const nix::ExprVar *E,
    const std::map<const nix::Expr *, const nix::Expr *> &ParentMap) {

  const auto *EnvExpr = searchEnvExpr(E, ParentMap);

  if (!EnvExpr)
    return nix::noPos;

  return displOf(EnvExpr, E->displ);
}

void collectSymbols(const nix::Expr *E, const ParentMap &ParentMap,
                    std::vector<nix::Symbol> &R) {
  if (!E)
    return;
  if (const auto *EA = dynamic_cast<const nix::ExprAttrs *>(E)) {
    if (EA->recursive) {
      // Recursive attrset has local bindings available
      std::transform(EA->attrs.begin(), EA->attrs.end(), std::back_inserter(R),
                     [](const auto &V) { return V.first; });
    }
  } else if (const auto *EL = dynamic_cast<const nix::ExprLet *>(E)) {
    std::transform(EL->attrs->attrs.begin(), EL->attrs->attrs.end(),
                   std::back_inserter(R),
                   [](const auto &V) { return V.first; });
  } else if (const auto *EF = dynamic_cast<const nix::ExprLambda *>(E)) {
    if (EF->arg)
      R.emplace_back(EF->arg);
    if (EF->hasFormals()) {
      std::transform(EF->formals->formals.begin(), EF->formals->formals.end(),
                     std::back_inserter(R),
                     [](const auto &V) { return V.name; });
    }
  }
  if (ParentMap.contains(E) && ParentMap.at(E) != E)
    collectSymbols(ParentMap.at(E), ParentMap, R);
}

} // namespace nixd
