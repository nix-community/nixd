#include "nixt/HookExpr.h"

namespace nixt {

#define NIX_EXPR(EXPR)                                                         \
  void Hook##EXPR::eval(nix::EvalState &State, nix::Env &Env, nix::Value &V) { \
    nix::EXPR::eval(State, Env, V);                                            \
    VMap[Handle] = V;                                                          \
    EMap[Handle] = &Env;                                                       \
  }
#include "nixt/Nodes.inc"
#undef NIX_EXPR

} // namespace nixt
