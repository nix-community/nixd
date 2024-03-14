#pragma once

#include <nix/nixexpr.hh>

#include <map>

namespace nixt {

using ValueMap = std::map<std::uintptr_t, nix::Value>;
using EnvMap = std::map<std::uintptr_t, nix::Env *>;

#define NIX_EXPR(EXPR)                                                         \
  struct Hook##EXPR : nix::EXPR {                                              \
    ValueMap &VMap;                                                            \
    EnvMap &EMap;                                                              \
    std::uintptr_t Handle;                                                     \
    Hook##EXPR(nix::EXPR E, ValueMap &VMap, EnvMap &EMap,                      \
               std::uintptr_t Handle)                                          \
        : nix::EXPR(E), VMap(VMap), EMap(EMap), Handle(Handle) {}              \
    void eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) override;   \
    std::string getName();                                                     \
  };
#include "Nodes.inc"
#undef NIX_EXPR

} // namespace nixt
