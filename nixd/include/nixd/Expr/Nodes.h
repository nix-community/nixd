#pragma once

#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>

namespace nixd::nodes {

struct StaticBindable {
  virtual void bindVarsStatic(nix::SymbolTable &Symbols,
                              nix::PosTable &Positions,
                              const nix::StaticEnv &Env) = 0;
};

#define NIX_EXPR(EXPR)                                                         \
  struct EXPR : StaticBindable, nix::EXPR {                                    \
    void bindVarsStatic(nix::SymbolTable &Symbols, nix::PosTable &Positions,   \
                        const nix::StaticEnv &Env) override;                   \
    using nix::EXPR::EXPR;                                                     \
  }; // namespace nixd::nodes
#include "Nodes.inc"
#undef NIX_EXPR

} // namespace nixd::nodes
