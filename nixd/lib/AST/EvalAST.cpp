#include "nixd/AST/EvalAST.h"
#include "nixd/Expr/CallbackExpr.h"
#include "nixd/Nix/EvalState.h"

namespace nixd {

void EvalAST::rewriteAST() {
  auto EvalCallback = [this](const nix::Expr *Expr, const nix::EvalState &,
                             nix::Env &ExprEnv, nix::Value &ExprValue) {
    ValueMap[Expr] = ExprValue;
    EnvMap[Expr] = &ExprEnv;
  };
  std::map<const nix::Expr *, nix::Expr *> OldNewMap;

  Root = rewriteCallback(Cxt, EvalCallback, Data->result, OldNewMap);

  // Reconstruct location info from the mapping
  for (auto [K, V] : Data->locations) {
    const auto *Expr = static_cast<const nix::Expr *>(K);
    if (OldNewMap.contains(Expr))
      Locations.insert({OldNewMap[Expr], V});
    else
      Locations.insert({K, V});
  }
}

nix::Value EvalAST::getValue(const nix::Expr *Expr) const {
  if (const auto *EV = dynamic_cast<const nix::ExprVar *>(Expr)) {
    if (!EV->fromWith) {
      const auto *EnvExpr = searchEnvExpr(EV, ParentMap);
      if (const auto *EL = dynamic_cast<const nix::ExprLambda *>(EnvExpr)) {
        const auto *F = EnvMap.at(EL->body);
        return *F->values[EV->displ];
      }
    }
  }
  return ValueMap.at(Expr);
}

void EvalAST::injectAST(nix::EvalState &State, lspserver::PathRef Path) const {
  if (!Data->error.empty())
    return;
  nix::Value DummyValue{};
  State.cacheFile(nix::CanonPath(Path.str()), nix::CanonPath(Path.str()), Root,
                  DummyValue);
}

nix::Value EvalAST::searchUpValue(const nix::Expr *Expr) const {
  for (;;) {
    if (ValueMap.contains(Expr))
      return ValueMap.at(Expr);
    if (parent(Expr) == Expr)
      break;

    Expr = parent(Expr);
  }
  throw std::out_of_range("No such value associated to ancestors");
}

nix::Env *EvalAST::searchUpEnv(const nix::Expr *Expr) const {
  for (;;) {
    if (EnvMap.contains(Expr))
      return EnvMap.at(Expr);
    if (parent(Expr) == Expr)
      break;

    Expr = parent(Expr);
  }
  throw std::out_of_range("No such env associated to ancestors");
}

nix::Value EvalAST::getValueEval(const nix::Expr *Expr,
                                 nix::EvalState &State) const {
  try {
    return getValue(Expr);
  } catch (std::out_of_range &) {
    // It is not evaluated.
    // Let's find evaluated parent, and try to eval it then.
    auto V = searchUpValue(Expr);
    forceValueDepth(State, V, 2);
    return getValue(Expr);
  }
}

} // namespace nixd
