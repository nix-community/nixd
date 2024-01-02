/// \file
/// \brief Describe how to traverse upon nix::Expr * nodes.
///
/// This file contains a CRTP base class for traversing nix::Expr * nodes.

#pragma once

#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>

/// \brief Library for playing with `nix::Expr` nodes.
///
/// This is a library with some utilities playing with nix AST nodes (e.g.
/// traversing, visiting, encoding, decoding, dispatching, printing). It is not
/// a parser, so you should use other libraries to parse nix code.

namespace nixt {

/// \brief A
/// [CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern)
/// base class for traversing `nix::Expr *` nodes.
///
/// Usage:
///
/// \code{.cpp}
/// struct MyVisitor : public RecursiveASTVisitor<MyVisitor> {
///   // This can be omitted.
///   bool traverseExpr(const nix::Expr *E) {
///     // Do something before/after traversing children.
///   }
///
///   // return `true` to traverse post-order, otherwise pre-order (default).
///   bool shouldTraversePostOrder() { return true; }
///
///   // sreturn `true` if we should continue traversing.
///   bool visitExprInt(const nix::ExprInt *E) { return true; }
///   bool visitExprFloat(const nix::ExprFloat *E) { return true; }
/// } V;
/// V.traverseExpr(Root); // call traverseExpr() on Root.
/// \endcode
///
/// \note This is based on dynamic_cast, so it is not very efficient.
///
/// `visit*()` methods are called once for each node.` traverse*()` methods are
/// automatically generated describing relations between nodes. Usually you
/// should always write custom `visit*()` methods, and only write `traverse*()`
/// methods when you need to do something special.
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
