#pragma once

#include "CallbackExpr.h"

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

  std::multimap<lspserver::Position, size_t> PosMap;

public:
  /// Inject myself into nix cache.
  void injectAST(nix::EvalState &State, lspserver::PathRef Path);

  /// From fresh-parsed nix::Expr *AST root, e.g. State->parseExprFromString
  EvalAST(nix::Expr *Root);

  /// Get the evaluation result (fixed point) of the expression.
  nix::Value getValue(nix::Expr *Expr) { return ValueMap.at(Expr); }

  /// Get the corresponding 'Env' while evaluating the expression.
  /// nix 'Env's contains dynamic variable name bindings at evaluation, might be
  /// used for completion.
  const nix::Env *getEnv(nix::Expr *Expr) { return EnvMap.at(Expr); }

  /// Lookup an AST node located at the position.
  /// Call 'preparePositionLookup' first.
  [[nodiscard]] nix::Expr *lookupPosition(lspserver::Position) const;

  void preparePositionLookup(const nix::EvalState &State);

  nix::Expr *root() { return Root; }
};

} // namespace nixd
