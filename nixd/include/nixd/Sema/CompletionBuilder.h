#pragma once

#include "nixd/AST/EvalAST.h"
#include "nixd/AST/ParseAST.h"

#include "lspserver/Protocol.h"

#include <utility>
#include <vector>

namespace nixd {

using CompletionResult = lspserver::CompletionList;

class CompletionBuilder {
  CompletionResult Result;
  size_t Limit = 1000;
  std::string Prefix;

  void addItem(lspserver::CompletionItem Item);

public:
  /// { a = 1; b = 2; }.|
  ///                   ^ "a", "b"
  void addAttrFields(const EvalAST &AST, const lspserver::Position &Pos,
                     nix::EvalState &State);

  /// Symbols from let-bindings, rec-attrs.
  void addSymbols(const ParseAST &AST, const nix::Expr *Node);

  ///
  /// (
  /// { foo ? 1 # <-- Lambda formal
  /// , bar ? 2
  /// }: foo + bar) { | }
  ///                 ^ "foo", "bar"
  void addLambdaFormals(const EvalAST &AST, nix::EvalState &State,
                        const nix::Expr *Node);

  /// with pkgs; [ ]
  void addEnv(nix::EvalState &State, nix::Env &NixEnv);

  void addEnv(const EvalAST &AST, nix::EvalState &State, const nix::Expr *Node);

  /// builtins.*
  void addStaticEnv(const nix::SymbolTable &STable, const nix::StaticEnv &SEnv);

  /// Complete option items, for Nixpkgs option system.
  void addOption(nix::EvalState &State, nix::Value &OptionAttrSet,
                 const std::vector<std::string> &AttrPath);

  CompletionResult &getResult() { return Result; }

  void setLimit(size_t Limit) { this->Limit = Limit; }

  void setPrefix(std::string Prefix) { this->Prefix = std::move(Prefix); }
};

} // namespace nixd
