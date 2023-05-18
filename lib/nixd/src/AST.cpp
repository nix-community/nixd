#include "nixd/AST.h"
#include "nixd/CallbackExpr.h"

#include "lspserver/Logger.h"
#include "nixd/Expr.h"

#include <exception>
#include <memory>

namespace nixd {

EvalAST::EvalAST(nix::Expr *Root) : Cxt(ASTContext()) {
  auto EvalCallback = [this](const nix::Expr *Expr, const nix::EvalState &,
                             const nix::Env &ExprEnv,
                             const nix::Value &ExprValue) {
    ValueMap[Expr] = ExprValue;
    EnvMap[Expr] = ExprEnv;
  };
  this->Root = rewriteCallback(Cxt, EvalCallback, Root);
}

void EvalAST::injectAST(nix::EvalState &State, lspserver::PathRef Path) {
  nix::Value DummyValue{};
  State.cacheFile(Path.str(), Path.str(), Root, DummyValue);
}

} // namespace nixd
