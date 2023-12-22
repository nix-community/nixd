#pragma once

#include "lspserver/Protocol.h"

#include "nixd/Expr/Nodes.h"
#include "nixd/Parser/Parser.h"
#include "nixd/Support/Position.h"

#include <nixt/Displacement.h>
#include <nixt/ParentMap.h>

#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>
#include <optional>
#include <stdexcept>

namespace nixd {

// Static Analysis (without any eval stuff)
class ParseAST {
public:
  using Definition = std::pair<const nix::Expr *, nix::Displacement>;
  using TextEdits = std::vector<lspserver::TextEdit>;
  using Symbols = std::vector<lspserver::DocumentSymbol>;
  using Links = std::vector<lspserver::DocumentLink>;

  enum class LocationContext {

    /// Expecting an attr name, not values.
    /// {
    ///    |
    ///    ^
    /// }
    AttrName,

    /// Expecting a nix value.
    /// {
    ///    a = |
    ///        ^
    /// }
    Value,

    Unknown
  };

protected:
  std::unique_ptr<ParseData> Data;
  std::map<const nix::Expr *, const nix::Expr *> ParentMap;
  std::map<Definition, std::vector<const nix::ExprVar *>> References;
  std::map<const nix::ExprVar *, Definition> Definitions;

  ParseAST(std::unique_ptr<ParseData> D) : Data(std::move(D)) {}

  void prepareDefRef();

  void staticAnalysis() {
    ParentMap = nixt::parentMap(root());
    prepareDefRef();
  }

  void bindVarsStatic(const nix::StaticEnv &Env) {
    if (auto *Root = dynamic_cast<nodes::StaticBindable *>(Data->result)) {
      Root->bindVarsStatic(symbols(), positions(), Env);
    }
  }

  void bindVarsStatic() {
    nix::StaticEnv Env(true, nullptr);
    bindVarsStatic(Env);
  }

public:
  static std::unique_ptr<ParseAST> create(std::unique_ptr<ParseData> D) {
    auto AST = std::unique_ptr<ParseAST>(new ParseAST(std::move(D)));
    AST->bindVarsStatic();
    AST->staticAnalysis();
    return AST;
  }

  virtual ~ParseAST() = default;

  [[nodiscard]] const nix::PosTable &positions() const { return *Data->PTable; }
  [[nodiscard]] const nix::SymbolTable &symbols() const {
    return *Data->STable;
  }

  [[nodiscard]] virtual nix::Expr *root() const { return Data->result; }

  [[nodiscard]] virtual nix::PosIdx getPos(const void *Ptr) const {
    return Data->locations.at(Ptr);
  }

  /// Get the parent of some expr, if it is root, \return Expr itself
  const nix::Expr *parent(const nix::Expr *Expr) const {
    if (ParentMap.contains(Expr))
      return ParentMap.at(Expr);
    return nullptr;
  };

  /// Find the expression that created 'Env' for ExprVar
  const nix::Expr *envExpr(const nix::ExprVar *Var) const {
    return searchEnvExpr(Var, ParentMap);
  }

  [[nodiscard]] nix::PosIdx end(nix::PosIdx P) const { return Data->end.at(P); }

  [[nodiscard]] Range nPair(nix::PosIdx P) const {
    return {nPairIdx(P), positions()};
  }

  [[nodiscard]] RangeIdx nPairIdx(nix::PosIdx P) const { return {P, end(P)}; }

  [[nodiscard]] bool contains(nix::PosIdx P,
                              const lspserver::Position &Pos) const {
    try {
      lspserver::Range Range = nPair(P);
      return Range.contains(Pos);
    } catch (std::out_of_range &) {
      return false;
    }
  }

  std::optional<Definition> searchDef(const nix::ExprVar *Var) const;

  [[nodiscard]] std::vector<const nix::ExprVar *> ref(Definition D) const {
    return References.at(D);
  }

  [[nodiscard]] Definition def(const lspserver::Position &Pos) const {
    if (const auto *Node = lookupContainMin(Pos)) {
      if (const auto *EVar = dynamic_cast<const nix::ExprVar *>(Node)) {
        return def(EVar);
      }
    }
    throw std::out_of_range("AST: no suitable definition");
  }

  Definition def(const nix::ExprVar *Var) const { return Definitions.at(Var); }

  [[nodiscard]] Range defRange(Definition Def) const {
    auto [E, Displ] = Def;
    return nPair(nixt::displOf(E, Displ));
  }

  std::optional<lspserver::Range> lRange(const void *Ptr) const noexcept {
    try {
      return toLSPRange(nRange(Ptr));
    } catch (...) {
      return std::nullopt;
    }
  }

  Range nRange(const void *Ptr) const { return {nRangeIdx(Ptr), positions()}; }

  RangeIdx nRangeIdx(const void *Ptr) const { return {getPos(Ptr), Data->end}; }

  [[nodiscard]] std::optional<Definition>
  lookupDef(lspserver::Position Desired) const;

  /// Lookup an AST node that ends before or on the cursor.
  /// { }  |
  ///   ^
  /// @returns nullptr, if not found
  [[nodiscard]] const nix::Expr *
  lookupEnd(lspserver::Position Desired) const noexcept;

  /// Lookup AST nodes that contains the cursor
  /// { |     }
  /// ^~~~~~~~^
  ///
  /// @returns empty vector, if there is no such expression
  [[nodiscard]] std::vector<const nix::Expr *>
  lookupContain(lspserver::Position Desired) const noexcept;

  [[nodiscard]] const nix::Expr *
  lookupContainMin(lspserver::Position Desired) const noexcept;

  /// Lookup an AST node that starts after or on the cursor
  /// |  { }
  ///    ^
  ///
  /// @returns nullptr, if not found
  [[nodiscard]] const nix::Expr *lookupStart(lspserver::Position Desired) const;

  void collectSymbols(const nix::Expr *E, std::vector<nix::Symbol> &R) const {
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

  [[nodiscard]] std::optional<TextEdits>
  rename(lspserver::Position Pos, const std::string &NewName) const {
    if (const auto *EVar =
            dynamic_cast<const nix::ExprVar *>(lookupContainMin(Pos))) {
      return rename(EVar, NewName);
    }
    if (auto Def = lookupDef(Pos)) {
      return rename(*Def, NewName);
    }
    return std::nullopt;
  };

  // Document Symbol
  [[nodiscard]] Symbols documentSymbol() const;

  [[nodiscard]] Links documentLink(const std::string &File) const;

  [[nodiscard]] LocationContext getContext(lspserver::Position Pos) const;
};
} // namespace nixd
