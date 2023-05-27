/// 'nix::Expr' wrapper that suitable for language server
#pragma once

#include <nix/nixexpr.hh>

namespace nixd {

/// Holds AST Nodes.
class ASTContext {
public:
  std::vector<std::unique_ptr<nix::Expr>> Nodes;
  template <class T> T *addNode(std::unique_ptr<T> Node) {
    Nodes.push_back(std::move(Node));
    return dynamic_cast<T *>(Nodes.back().get());
  }
};

template <class Derived> struct RecursiveASTVisitor {

  bool shouldTraversePostOrder() { return false; }

#define NIX_EXPR(EXPR) bool traverse##EXPR(const nix::EXPR *E);
#include "NixASTNodes.inc"
#undef NIX_EXPR

#define NIX_EXPR(EXPR)                                                         \
  bool visit##EXPR(const nix::EXPR *) { return true; }
#include "NixASTNodes.inc"
#undef NIX_EXPR

  Derived &getDerived() { return *static_cast<Derived *>(this); }

  bool traverseExpr(const nix::Expr *E) {
    if (!E)
      return true;
#define NIX_EXPR(EXPR)                                                         \
  if (auto CE = dynamic_cast<const nix::EXPR *>(E)) {                          \
    return getDerived().traverse##EXPR(CE);                                    \
  }
#include "NixASTNodes.inc"
    assert(false && "We are missing some nix AST Nodes!");
    return true;
#undef NIX_EXPR
  }
}; // namespace nixd

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
#include "NixASTTraverse.inc"
#undef DEF_TRAVERSE_TYPE
#undef TRY_TO_TRAVERSE
#undef TRY_TO

inline const char *getExprName(nix::Expr *E) {
#define NIX_EXPR(EXPR)                                                         \
  if (dynamic_cast<const nix::EXPR *>(E)) {                                    \
    return #EXPR;                                                              \
  }
#include "NixASTNodes.inc"
#undef NIX_EXPR
}

} // namespace nixd
