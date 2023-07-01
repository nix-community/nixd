#pragma once

#include "lspserver/Protocol.h"

#include "nixd/Parser/Parser.h"
#include "nixd/Support/Position.h"

#include <nix/nixexpr.hh>

namespace nixd {

// Static Analysis (without any eval stuff)
class ParseAST {
public:
  using Definition = std::pair<const nix::Expr *, nix::Displacement>;
  using TextEdits = std::vector<lspserver::TextEdit>;

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

  // Rename

  [[nodiscard]] lspserver::TextEdit edit(Definition D,
                                         const std::string &NewName) const {
    return {defRange(D), NewName};
  };

  [[nodiscard]] TextEdits edit(const std::vector<const nix::ExprVar *> &Refs,
                               const std::string &NewName) const;

  [[nodiscard]] TextEdits rename(Definition D,
                                 const std::vector<const nix::ExprVar *> &Refs,
                                 const std::string &NewName) const {
    auto Edits = edit(Refs, NewName);
    Edits.emplace_back(edit(D, NewName));
    return Edits;
  }

  [[nodiscard]] TextEdits rename(Definition D,
                                 const std::string &NewName) const {
    return rename(D, ref(D), NewName);
  };

  [[nodiscard]] TextEdits rename(const nix::ExprVar *Var,
                                 const std::string &NewName) const {
    return rename(def(Var), NewName);
  };
};
} // namespace nixd
