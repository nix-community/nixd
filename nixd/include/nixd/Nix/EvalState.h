#pragma once

#include <nix/eval.hh>

// Our extension to nix::EvalState.
namespace nix {
/// Similar to nix forceValue, but allow a depth limit
void forceValueDepth(EvalState &State, Value &v, int depth);
} // namespace nix
