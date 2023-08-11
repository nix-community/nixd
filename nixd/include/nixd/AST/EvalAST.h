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
  EvalAST(std::unique_ptr<ParseData> D) : ParseAST(std::move(D)) {
    rewriteAST();
    staticAnalysis();
  }

  [[nodiscard]] nix::PosIdx getPos(const void *Ptr) const override {
    return Locations.at(Ptr);
  }

  [[nodiscard]] nix::Expr *root() const override { return Root; }

  /// Inject myself into nix cache.
  void injectAST(nix::EvalState &State, lspserver::PathRef Path) const;

  std::optional<nix::Value> searchUpValue(const nix::Expr *Expr) const;

  /// Try to search (traverse) up the expr and find the first `Env` associated
  /// ancestor, return its env
  nix::Env *searchUpEnv(const nix::Expr *Expr) const;

  std::optional<nix::Value>
  getValueStatic(const nix::Expr *Expr) const noexcept;

  /// Get the evaluation result (fixed point) of the expression.
  std::optional<nix::Value> getValue(const nix::Expr *Expr) const noexcept;

  std::optional<nix::Value> getValueEval(const nix::Expr *Expr,
                                         nix::EvalState &State) const noexcept;

  /// Get the corresponding 'Env' while evaluating the expression.
  /// nix 'Env's contains dynamic variable name bindings at evaluation, might
  /// be used for completion.
  nix::Env *getEnv(const nix::Expr *Expr) const { return EnvMap.at(Expr); }
};

} // namespace nixd
