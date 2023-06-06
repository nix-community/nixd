#include "nixd/Expr.h"

#include <nix/nixexpr.hh>

namespace nixd {

std::map<const nix::Expr *, const nix::Expr *>
getParentMap(const nix::Expr *Root) {
  decltype(getParentMap(nullptr)) Ret;
  struct VisitorClass : RecursiveASTVisitor<VisitorClass> {
    /// The parent before traverseExpr
    const nix::Expr *ParentExpr;
    decltype(Ret) *CapturedRet;

    bool traverseExpr(const nix::Expr *E) {
      CapturedRet->insert({E, ParentExpr});
      const auto *OldParent = ParentExpr;
      ParentExpr = E; // Set the parent into the visitor, it should be the
                      // parent when we are traversing child nodes.
      if (!RecursiveASTVisitor<VisitorClass>::traverseExpr(E))
        return false;

      // After traversing on childrens finished, set parent expr to previous
      // parent.
      ParentExpr = OldParent;
      return true;
    }

  } Visitor;

  Visitor.ParentExpr = Root;
  Visitor.CapturedRet = &Ret;

  Visitor.traverseExpr(Root);

  return Ret; // NRVO
}

nix::PosIdx getDisplOf(const nix::Expr *E, nix::Displacement Displ) {
  if (const auto *CE = dynamic_cast<const nix::ExprAttrs *>(E))
    return getDisplOf(CE, Displ);
  if (const auto *CE = dynamic_cast<const nix::ExprLet *>(E))
    return getDisplOf(CE, Displ);
  if (const auto *CE = dynamic_cast<const nix::ExprLambda *>(E))
    return getDisplOf(CE, Displ);

  assert(false && "The requested expr is not an env creator");
  return nix::noPos; // unreachable
}

nix::PosIdx getDisplOf(const nix::ExprAttrs *E, nix::Displacement Displ) {
  assert(E->recursive && "Only recursive ExprAttr has displacement values");

  auto DefIt = E->attrs.begin();
  std::advance(DefIt, Displ);

  return DefIt->second.pos;
}

nix::PosIdx getDisplOf(const nix::ExprLet *E, nix::Displacement Displ) {
  auto DefIt = E->attrs->attrs.begin();
  std::advance(DefIt, Displ);

  return DefIt->second.pos;
}

nix::PosIdx getDisplOf(const nix::ExprLambda *E, nix::Displacement Displ) {
  // this is broken because of:

  //         newEnv->sort();

  // So we cannot now which position associated the Displ, *BEFORE* sorting.
  return nix::noPos;
}

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
  if (const auto *CE = dynamic_cast<const nix::ExprLambda *>(E))
    return true;

  // src/libexpr/nixexpr.cc:472
  if (const auto *CE = dynamic_cast<const nix::ExprLet *>(E))
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

    assert(ParentExpr != E &&
           "Searched at the root of AST, but the Level still requested up");

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

  assert(EnvExpr && "EnvExpr must be non-null!");

  return getDisplOf(EnvExpr, E->displ);
}

} // namespace nixd
