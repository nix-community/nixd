/// 'nix::Expr' wrapper that suitable for language server
#pragma once

#include "Nodes.h"

#include <nixt/ParentMap.h>

#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>

namespace nixd {

/// RAII Pool that holds Nodes.
template <class T> class Context {
public:
  std::vector<std::unique_ptr<T>> Nodes;
  template <class U> U *addNode(std::unique_ptr<U> Node) {
    Nodes.push_back(std::move(Node));
    return dynamic_cast<U *>(Nodes.back().get());
  }
  template <class U> U *record(U *Node) {
    Nodes.emplace_back(std::unique_ptr<U>(Node));
    return Node;
  }
};

using ASTContext = Context<nix::Expr>;

/// For `ExprVar`s statically look up in `Env`s (i.e. !fromWith), search the
/// position
nix::PosIdx searchDefinition(const nix::ExprVar *,
                             const nixt::ParentMap &ParentMap);

const nix::Expr *searchEnvExpr(const nix::ExprVar *,
                               const nixt::ParentMap &ParentMap);

//-----------------------------------------------------------------------------/
// bool isEnvCreated (Expr* Parent, Expr Child)
//
// Check that if `Parent` created a env for the `Child`.

bool isEnvCreated(const nix::Expr *, const nix::Expr *);

bool isEnvCreated(const nix::ExprAttrs *, const nix::Expr *);

bool isEnvCreated(const nix::ExprWith *, const nix::Expr *);

//-----------------------------------------------------------------------------/

/// Statically collect available symbols in expression's scope.
void collectSymbols(const nix::Expr *, const nixt::ParentMap &,
                    std::vector<nix::Symbol> &);

} // namespace nixd
