#include "nixd/CallbackExpr.h"
#include "nixd/Expr.h"
#include "nixexpr.hh"
#include <memory>

namespace nixd {

#define NIX_EXPR(EXPR)                                                         \
  Callback##EXPR *Callback##EXPR::create(                                      \
      CallbackASTContext &Cxt, const nix::EXPR &E, ExprCallback ECB) {         \
                                                                               \
    return Cxt.addNode<Callback##EXPR>(                                        \
        std::make_unique<Callback##EXPR>(E, ECB));                             \
  }
#include "nixd/NixASTNodes.inc"
#undef NIX_EXPR

nix::Expr *rewriteCallback(CallbackASTContext &Cxt, ExprCallback ECB,
                           const nix::Expr *Root) {
#define TRY_TO_TRAVERSE(CHILD)                                                 \
  do {                                                                         \
    CHILD = dynamic_cast<std::remove_reference<decltype(CHILD)>::type>(        \
        rewriteCallback(Cxt, ECB, CHILD));                                     \
  } while (false)

#define DEF_TRAVERSE_TYPE(TYPE, CODE)                                          \
  if (const auto *E = dynamic_cast<const nix::TYPE *>(Root)) {                 \
    auto *T = Callback##TYPE::create(Cxt, *E, ECB);                            \
    { CODE; }                                                                  \
    return T;                                                                  \
  }
#include "nixd/NixASTTraverse.inc"
  return nullptr;
#undef TRY_TO_TRAVERSE
#undef DEF_TRAVERSE_TYPE
}
#define NIX_EXPR(EXPR)                                                         \
  void Callback##EXPR::eval(nix::EvalState &State, nix::Env &Env,              \
                            nix::Value &V) {                                   \
    nix::EXPR::eval(State, Env, V);                                            \
    ECB(this, State, Env, V);                                                  \
  }
#include "nixd/NixASTNodes.inc"
#undef NIX_EXPR

} // namespace nixd
