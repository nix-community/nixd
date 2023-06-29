#pragma once

#include "Expr.h"

#include <nix/eval.hh>

#include <memory>

namespace nixd {

using ExprCallback = std::function<void(nix::Expr *, nix::EvalState &,
                                        nix::Env &, nix::Value &)>;

// Nix expression subclass, that will call a readonly callback for analysis.
#define NIX_EXPR(EXPR)                                                         \
  struct Callback##EXPR : nix::EXPR {                                          \
    ExprCallback ECB;                                                          \
    Callback##EXPR(nix::EXPR E) : nix::EXPR(E) {}                              \
    Callback##EXPR(nix::EXPR E, ExprCallback ECB) : nix::EXPR(E), ECB(ECB) {}  \
    static Callback##EXPR *create(ASTContext &Cxt, const nix::EXPR &E,         \
                                  ExprCallback ECB);                           \
    void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;   \
    std::string getName();                                                     \
  };
#include "Nodes.inc"
#undef NIX_EXPR

/// Rewrite the AST, rooted at \p Root, \returns the root of the result tree.
/// Nodes are stored into \p Cxt
nix::Expr *rewriteCallback(ASTContext &Cxt, ExprCallback ECB,
                           const nix::Expr *Root,
                           std::map<const nix::Expr *, nix::Expr *> &OldNewMap);

} // namespace nixd
