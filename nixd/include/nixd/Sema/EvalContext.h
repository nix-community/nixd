#pragma once

#include "nixd/Support/GCPool.h"

#include <nix/nixexpr.hh>

namespace nixd {

struct EvalContext {
  using ES = std::vector<std::pair<nix::PosIdx, nix::Expr *>>;
  GCPool<nix::Expr> Pool;
  GCPool<nix::Formals> FormalsPool;
  GCPool<ES> ESPool;
};

} // namespace nixd
