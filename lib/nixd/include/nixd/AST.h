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
public:
  using Definition = std::pair<const nix::Expr *, nix::Displacement>;

protected:
  std::unique_ptr<ParseData> Data;
  std::map<const nix::Expr *, const nix::Expr *> ParentMap;
  std::map<Definition, std::vector<const nix::ExprVar *>> References;
  std::map<const nix::ExprVar *, Definition> Definitions;

public:
  void staticAnalysis() {
    ParentMap = getParentMap(root());
    prepareDefRef();
  }
  ParseAST(decltype(Data) D, bool DoSA = true) : Data(std::move(D)) {
    if (DoSA)
      staticAnalysis();
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

  [[nodiscard]] nix::PosIdx end(nix::PosIdx P) const { return Data->end.at(P); }

  [[nodiscard]] Range nPair(nix::PosIdx P) const {
    return {nPairIdx(P), Data->state.positions};
  }

  [[nodiscard]] RangeIdx nPairIdx(nix::PosIdx P) const { return {P, end(P)}; }

  void prepareDefRef();

  std::optional<Definition> searchDef(const nix::ExprVar *Var) const;

  [[nodiscard]] std::vector<const nix::ExprVar *> ref(Definition D) const {
    return References.at(D);
  }

  Definition def(const nix::ExprVar *Var) const { return Definitions.at(Var); }

  [[nodiscard]] Range defRange(Definition Def) const {
    auto [E, Displ] = Def;
    return nPair(getDisplOf(E, Displ));
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

  [[nodiscard]] std::optional<Definition>
  lookupDef(lspserver::Position Desired) const;

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

  void collectSymbols(const nix::Expr *E, std::vector<Symbol> &R) const {
    return ::nixd::collectSymbols(E, ParentMap, R);
  }
};

/// A Nix language AST wrapper that support language features for LSP.
class EvalAST : public ParseAST {
  ASTContext Cxt;
  nix::Expr *Root;
  std::map<const nix::Expr *, nix::Value> ValueMap;
  std::map<const nix::Expr *, nix::Env *> EnvMap;

  std::map<const void *, PosIdx> Locations;

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

  /// Get the corresponding 'Env' while evaluating the expression.
  /// nix 'Env's contains dynamic variable name bindings at evaluation, might be
  /// used for completion.
  nix::Env *getEnv(const nix::Expr *Expr) const { return EnvMap.at(Expr); }
};

} // namespace nixd
