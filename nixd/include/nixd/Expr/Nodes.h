#pragma once

#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>

namespace nixd::nodes {

struct StaticBindable {
  virtual void bindVarsStatic(const nix::SymbolTable &Symbols,
                              const nix::PosTable &Positions,
                              const nix::StaticEnv &Env) = 0;
};

// Nixd extension to the official parser, for error recovery
struct ExprError : StaticBindable, nix::Expr {
  void show(const nix::SymbolTable &Symbols, std::ostream &Str) const override {
    Str << "<nixd:err>";
  };
  void bindVarsStatic(const nix::SymbolTable &Symbols,
                      const nix::PosTable &Positions,
                      const nix::StaticEnv &Env) override{};
  void bindVars(nix::EvalState &,
                const std::shared_ptr<const nix::StaticEnv> &) override {}
  void eval(nix::EvalState &, nix::Env &, nix::Value &V) override {
    V.mkNull();
  }
};

#define NIX_EXPR(EXPR)                                                         \
  struct EXPR : StaticBindable, nix::EXPR {                                    \
    void bindVarsStatic(const nix::SymbolTable &Symbols,                       \
                        const nix::PosTable &Positions,                        \
                        const nix::StaticEnv &Env) override;                   \
    using nix::EXPR::EXPR;                                                     \
  }; // namespace nixd::nodes
#include "Nodes.inc"
#undef NIX_EXPR

} // namespace nixd::nodes
