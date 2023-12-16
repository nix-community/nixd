/// Visitor.h, describe how to traverse upon nix::Expr * nodes.
#pragma once

#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>

namespace nixt {

template <class Derived> struct RecursiveASTVisitor {

  bool shouldTraversePostOrder() { return false; }

  bool visitExpr(const nix::Expr *) { return true; }

#define NIX_EXPR(EXPR) bool traverse##EXPR(const nix::EXPR *E);
#include "Nodes.inc"
#undef NIX_EXPR

#define NIX_EXPR(EXPR)                                                         \
  bool visit##EXPR(const nix::EXPR *E) { return getDerived().visitExpr(E); }
#include "Nodes.inc"
#undef NIX_EXPR

  Derived &getDerived() { return *static_cast<Derived *>(this); }

  bool traverseExpr(const nix::Expr *E) {
    if (!E)
      return true;
#define NIX_EXPR(EXPR)                                                         \
  if (auto CE = dynamic_cast<const nix::EXPR *>(E)) {                          \
    return getDerived().traverse##EXPR(CE);                                    \
  }
#include "Nodes.inc"
#undef NIX_EXPR
    assert(false && "We are missing some nix AST Nodes!");
    return true;
  }
};

#define TRY_TO(CALL_EXPR)                                                      \
  do {                                                                         \
    if (!getDerived().CALL_EXPR)                                               \
      return false;                                                            \
  } while (false)

#define TRY_TO_TRAVERSE(EXPR) TRY_TO(traverseExpr(EXPR))

#define DEF_TRAVERSE_TYPE(TYPE, CODE)                                          \
  template <typename Derived>                                                  \
  bool RecursiveASTVisitor<Derived>::traverse##TYPE(const nix::TYPE *T) {      \
    if (!getDerived().shouldTraversePostOrder())                               \
      TRY_TO(visit##TYPE(T));                                                  \
    { CODE; }                                                                  \
    if (getDerived().shouldTraversePostOrder())                                \
      TRY_TO(visit##TYPE(T));                                                  \
    return true;                                                               \
  }
#include "Traverse.inc"
#undef DEF_TRAVERSE_TYPE
#undef TRY_TO_TRAVERSE

} // namespace nixt
