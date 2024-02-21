#pragma once

#include "HookExpr.h"
#include "PtrPool.h"

#include <nix/nixexpr.hh>

#include <string_view>

namespace nixt {

struct DeserializeContext {
  PtrPool<nix::Expr> &Pool;
  nix::SymbolTable &STable;
  nix::PosTable &PTable;
};

nix::Expr *deserializeHookable(std::string_view &Data, DeserializeContext &Ctx,
                               ValueMap &VMap, EnvMap &EMap);

} // namespace nixt
