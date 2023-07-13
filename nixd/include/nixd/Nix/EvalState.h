#pragma once

#include <nix/eval.hh>

// Our extension to nix::EvalState.
namespace nix {
/// Similar to nix forceValue, but allow a depth limit
void forceValueDepth(EvalState &State, Value &v, int depth);

Symbol getName(const AttrName &name, EvalState &state, Env &env);

Value evalAttrPath(EvalState &state, Value Base, Env &env,
                   const AttrPath &Path);
} // namespace nix
