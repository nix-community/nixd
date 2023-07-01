#pragma once

#include "ParseAST.h"

#include "lspserver/Path.h"

namespace nixd {

/// A Nix language AST wrapper that support language features for LSP.
class EvalAST : public ParseAST {
  ASTContext Cxt;
  nix::Expr *Root;
  std::map<const nix::Expr *, nix::Value> ValueMap;
  std::map<const nix::Expr *, nix::Env *> EnvMap;

  std::map<const void *, nix::PosIdx> Locations;

  /// Rewrite the AST to our own nodes, used for collecting information
  void rewriteAST();

public:
  EvalAST(decltype(Data) D) : ParseAST(std::move(D), false) {
    rewriteAST();
    staticAnalysis();
  }

  [[nodiscard]] nix::PosIdx getPos(const void *Ptr) const override {
    return Locations.at(Ptr);
  }

  [[nodiscard]] nix::Expr *root() const override { return Root; }

  /// Inject myself into nix cache.
  void injectAST(nix::EvalState &State, lspserver::PathRef Path) const;

  /// Try to search (traverse) up the expr and find the first `Env` associated
  /// ancestor, return its env
  nix::Env *searchUpEnv(const nix::Expr *Expr) const;

  /// Similar to `searchUpEnv`, but search for Values
  nix::Value searchUpValue(const nix::Expr *Expr) const;

  /// Get the evaluation result (fixed point) of the expression.
  nix::Value getValue(const nix::Expr *Expr) const;

  nix::Value getValueEval(const nix::Expr *Expr, nix::EvalState &State) const;

  /// Get the corresponding 'Env' while evaluating the expression.
  /// nix 'Env's contains dynamic variable name bindings at evaluation, might
  /// be used for completion.
  nix::Env *getEnv(const nix::Expr *Expr) const { return EnvMap.at(Expr); }
};

} // namespace nixd
