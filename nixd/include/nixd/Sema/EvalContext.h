#pragma once

#include "nixd/Support/GCPool.h"

#include <nix/nixexpr.hh>

namespace nixd {

struct EvalContext {
  GCPool<nix::Expr> Pool;
  GCPool<nix::Formals> FormalsPool;
};

} // namespace nixd
