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

std::optional<nix::Value>
EvalAST::getValue(const nix::Expr *Expr) const noexcept {
  if (auto OpStaticValue = getValueStatic(Expr))
    return OpStaticValue;
  if (ValueMap.contains(Expr))
    return ValueMap.at(Expr);
  return std::nullopt;
}

void EvalAST::injectAST(nix::EvalState &State, lspserver::PathRef Path) const {
  if (!Data->error.empty())
    return;
  nix::Value DummyValue{};
  State.cacheFile(nix::CanonPath(Path.str()), nix::CanonPath(Path.str()), Root,
                  DummyValue);
}

std::optional<nix::Value> EvalAST::searchUpValue(const nix::Expr *Expr) const {
  for (;;) {
    if (ValueMap.contains(Expr))
      return ValueMap.at(Expr);
    if (parent(Expr) == Expr)
      break;

    Expr = parent(Expr);
  }
  return std::nullopt;
}

nix::Env *EvalAST::searchUpEnv(const nix::Expr *Expr) const {
  for (;;) {
    if (EnvMap.contains(Expr))
      return EnvMap.at(Expr);
    if (parent(Expr) == Expr)
      break;

    Expr = parent(Expr);
  }
  return nullptr;
}

std::optional<nix::Value>
EvalAST::getValueEval(const nix::Expr *Expr,
                      nix::EvalState &State) const noexcept {
  auto OpVCurrent = getValue(Expr);
  if (OpVCurrent)
    return OpVCurrent;

  // It is not evaluated.
  // Let's find evaluated parent, and try to eval it then.
  auto OpVAncestor = searchUpValue(Expr);
  if (!OpVAncestor)
    return std::nullopt;

  // TODO: add tests
  try {
    forceValueDepth(State, *OpVAncestor, 2);
  } catch (...) {
  }
  return getValue(Expr);
}

std::optional<nix::Value>
EvalAST::getValueStatic(const nix::Expr *Expr) const noexcept {
  const auto *EVar = dynamic_cast<const nix::ExprVar *>(Expr);
  if (!EVar)
    return std::nullopt;

  if (EVar->fromWith)
    return std::nullopt;

  const auto *EnvExpr = searchEnvExpr(EVar, ParentMap);

  // TODO: split this method, and make rec { }, let-binding available
  const auto *ELambda = dynamic_cast<const nix::ExprLambda *>(EnvExpr);
  if (!ELambda)
    return std::nullopt;

  if (!EnvMap.contains(ELambda->body))
    return std::nullopt;
  const auto *F = EnvMap.at(ELambda->body);
  return *F->values[EVar->displ];
}
std::unique_ptr<EvalAST> EvalAST::create(std::unique_ptr<ParseData> ParseData,
                                         nix::EvalState &State) {
  auto Ret = std::unique_ptr<EvalAST>(new EvalAST(std::move(ParseData)));
  if (Ret->Data->result && Ret->Data->error.empty())
    Ret->Data->result->bindVars(State, State.staticBaseEnv);
  Ret->rewriteAST();
  Ret->staticAnalysis();
  return Ret;
}
} // namespace nixd
