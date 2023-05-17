#pragma once

#include "Expr.h"
#include "eval.hh"
#include <memory>

namespace nixd {

using ExprCallback =
    std::function<void(const nix::Expr *, const nix::EvalState &,
                       const nix::Env &, const nix::Value &)>;

/// Holds AST Nodes.
class ASTContext {
  std::vector<std::unique_ptr<nix::Expr>> Nodes;

public:
  template <class T> T *addNode(std::unique_ptr<T> Node) {
    Nodes.push_back(std::move(Node));
    return dynamic_cast<T *>(Nodes.back().get());
  }
};

// Nix expression subclass, that will call a readonly callback for analysis.
#define NIX_EXPR(EXPR)                                                         \
  struct Callback##EXPR : nix::EXPR {                                          \
    ExprCallback ECB;                                                          \
    Callback##EXPR(nix::EXPR E) : nix::EXPR(E) {}                              \
    Callback##EXPR(nix::EXPR E, ExprCallback ECB) : nix::EXPR(E), ECB(ECB) {}  \
    static Callback##EXPR *create(ASTContext &Cxt, const nix::EXPR &E,         \
                                  ExprCallback ECB);                           \
    void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V);            \
  };
#include "NixASTNodes.inc"
#undef NIX_EXPR

/// Evaluate the "Expr", and associate the AST with values.
class ValueTree {};

/// Rewrite the AST, rooted at \p Root, \returns the root of the result tree.
/// Nodes are stored into \p Cxt
nix::Expr *rewriteCallback(ASTContext &Cxt, ExprCallback ECB,
                           const nix::Expr *Root);

} // namespace nixd
