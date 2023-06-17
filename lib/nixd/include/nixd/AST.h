#pragma once

#include "nixd/CallbackExpr.h"
#include "nixd/Expr.h"
#include "nixd/Parser.h"

#include "lspserver/Path.h"
#include "lspserver/Protocol.h"
#include "nixd/Position.h"

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
#include <optional>
#include <queue>
#include <utility>

namespace nixd {

// Static Analysis (without any eval stuff)
class ParseAST {
protected:
  std::unique_ptr<ParseData> Data;
  std::map<const nix::Expr *, const nix::Expr *> ParentMap;

public:
  ParseAST(decltype(Data) D) : Data(std::move(D)) {
    ParentMap = getParentMap(root());
  }

  virtual ~ParseAST() = default;

  [[nodiscard]] virtual nix::Expr *root() const { return Data->result; }

  [[nodiscard]] virtual nix::PosIdx getPos(const void *Ptr) const {
    return Data->locations.at(Ptr);
  }

  /// Get the parent of some expr, if it is root, \return Expr itself
  const nix::Expr *parent(const nix::Expr *Expr) const {
    return ParentMap.at(Expr);
  };

  /// Find the expression that created 'Env' for ExprVar
  const nix::Expr *envExpr(const nix::ExprVar *Var) const {
    return searchEnvExpr(Var, ParentMap);
  }

  nix::PosIdx definition(const nix::ExprVar *Var) const {
    return searchDefinition(Var, ParentMap);
  }

  std::optional<lspserver::Range> lRange(const void *Ptr) const {
    try {
      return toLSPRange(nRange(Ptr));
    } catch (...) {
      return std::nullopt;
    }
  }

  Range nRange(const void *Ptr) const {
    return {nRangeIdx(Ptr), Data->state.positions};
  }

  RangeIdx nRangeIdx(const void *Ptr) const { return {getPos(Ptr), Data->end}; }

  /// Lookup an AST node that ends before or on the cursor.
  /// { }  |
  ///   ^
  [[nodiscard]] const nix::Expr *lookupEnd(lspserver::Position Desired) const;

  /// Lookup AST nodes that contains the cursor
  /// { |     }
  /// ^~~~~~~~^
  [[nodiscard]] std::vector<const nix::Expr *>
  lookupContain(lspserver::Position Desired) const;

  [[nodiscard]] const nix::Expr *
  lookupContainMin(lspserver::Position Desired) const;

  /// Lookup an AST node that starts after or on the cursor
  /// |  { }
  ///    ^
  [[nodiscard]] const nix::Expr *lookupStart(lspserver::Position Desired) const;
};

/// A Nix language AST wrapper that support language features for LSP.
class EvalAST : public ParseAST {
  ASTContext Cxt;
  nix::Expr *Root;
  std::map<const nix::Expr *, nix::Value> ValueMap;
  std::map<const nix::Expr *, const nix::Env *> EnvMap;

  std::map<const void *, PosIdx> Locations;

  /// Rewrite the AST to our own nodes, used for collecting information
  void rewriteAST();

public:
  EvalAST(decltype(Data) D) : ParseAST(std::move(D)) {
    rewriteAST();
    ParentMap = getParentMap(root());
  }

  [[nodiscard]] nix::PosIdx getPos(const void *Ptr) const override {
    return Locations.at(Ptr);
  }

  [[nodiscard]] nix::Expr *root() const override { return Root; }

  /// Inject myself into nix cache.
  void injectAST(nix::EvalState &State, lspserver::PathRef Path) const;

  /// Try to search (traverse) up the expr and find the first `Env` associated
  /// ancestor, return its env
  const nix::Env *searchUpEnv(const nix::Expr *Expr) const;

  /// Similar to `searchUpEnv`, but search for Values
  nix::Value searchUpValue(const nix::Expr *Expr) const;

  /// Get the evaluation result (fixed point) of the expression.
  nix::Value getValue(const nix::Expr *Expr) const;

  /// Get the corresponding 'Env' while evaluating the expression.
  /// nix 'Env's contains dynamic variable name bindings at evaluation, might be
  /// used for completion.
  const nix::Env *getEnv(const nix::Expr *Expr) const {
    return EnvMap.at(Expr);
  }
};

} // namespace nixd
