#pragma once

#include "nixd/CallbackExpr.h"
#include "nixd/Expr.h"

#include "lspserver/Path.h"
#include "lspserver/Protocol.h"

#include <llvm/ADT/FunctionExtras.h>
#include <llvm/ADT/StringRef.h>

#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/installable-value.hh>
#include <nix/installables.hh>
#include <nix/nixexpr.hh>
#include <nix/ref.hh>

#include <boost/asio.hpp>

#include <map>
#include <memory>
#include <queue>
#include <utility>

namespace nixd {

/// A Nix language AST wrapper that support language features for LSP.
/// Most features are available after evaluation.
class EvalAST {
  ASTContext Cxt;
  nix::Expr *Root;
  std::map<const nix::Expr *, nix::Value> ValueMap;
  std::map<const nix::Expr *, const nix::Env *> EnvMap;

  std::map<const nix::Expr *, const nix::Expr *> ParentMap;

  std::multimap<lspserver::Position, const nix::Expr *> PosMap;

public:
  /// Inject myself into nix cache.
  void injectAST(nix::EvalState &State, lspserver::PathRef Path);

  /// From fresh-parsed nix::Expr *AST root, e.g. State->parseExprFromString
  EvalAST(nix::Expr *Root);

  /// Get the evaluation result (fixed point) of the expression.
  nix::Value getValue(const nix::Expr *Expr) const { return ValueMap.at(Expr); }

  /// Get the corresponding 'Env' while evaluating the expression.
  /// nix 'Env's contains dynamic variable name bindings at evaluation, might be
  /// used for completion.
  const nix::Env *getEnv(const nix::Expr *Expr) const {
    return EnvMap.at(Expr);
  }

  /// Get the parent of some expr, if it is root, \return Expr itself
  const nix::Expr *parent(const nix::Expr *Expr) const {
    return ParentMap.at(Expr);
  };

  void prepareParentTable() { ParentMap = getParentMap(Root); }

  /// Try to search (traverse) up the expr and find the first `Env` associated
  /// ancestor, return its env
  const nix::Env *searchUpEnv(const nix::Expr *Expr) const;

  /// Similar to `searchUpEnv`, but search for Values
  nix::Value searchUpValue(const nix::Expr *Expr) const;

  /// Find the expression that created 'Env' for ExprVar
  const nix::Expr *envExpr(const nix::ExprVar *Var) const {
    return searchEnvExpr(Var, ParentMap);
  }

  nix::PosIdx definition(const nix::ExprVar *Var) const {
    return searchDefinition(Var, ParentMap);
  }

  /// Lookup an AST node located at the position.
  /// Call 'preparePositionLookup' first.
  [[nodiscard]] const nix::Expr *lookupPosition(lspserver::Position) const;

  void preparePositionLookup(const nix::EvalState &State);

  nix::Expr *root() { return Root; }
};

} // namespace nixd
