#include "nixd/Expr/CallbackExpr.h"
#include "nixd/Expr/Expr.h"

#include <nix/nixexpr.hh>

#include <memory>

namespace nixd {

#define NIX_EXPR(EXPR)                                                         \
  Callback##EXPR *Callback##EXPR::create(ASTContext &Cxt, const nix::EXPR &E,  \
                                         ExprCallback ECB) {                   \
                                                                               \
    return Cxt.addNode<Callback##EXPR>(                                        \
        std::make_unique<Callback##EXPR>(E, ECB));                             \
  }
#include "nixd/Expr/NixASTNodes.inc"
#undef NIX_EXPR

nix::Expr *
rewriteCallback(ASTContext &Cxt, ExprCallback ECB, const nix::Expr *Root,
                std::map<const nix::Expr *, nix::Expr *> &OldNewMap) {
#define TRY_TO_TRAVERSE(CHILD)                                                 \
  do {                                                                         \
    CHILD = dynamic_cast<std::remove_reference<decltype(CHILD)>::type>(        \
        rewriteCallback(Cxt, ECB, CHILD, OldNewMap));                          \
  } while (false)

#define DEF_TRAVERSE_TYPE(TYPE, CODE)                                          \
  if (const auto *E = dynamic_cast<const nix::TYPE *>(Root)) {                 \
    auto *T = Callback##TYPE::create(Cxt, *E, ECB);                            \
    OldNewMap.insert({E, T});                                                  \
    { CODE; }                                                                  \
    return T;                                                                  \
  }
#include "nixd/Expr/NixASTTraverse.inc"
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
#include "nixd/Expr/NixASTNodes.inc"
#undef NIX_EXPR

} // namespace nixd
