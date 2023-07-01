#include "nixd/Expr/Nodes.h"

#include <nix/nixexpr.hh>

#include <cassert>
#include <optional>

namespace nixd::nodes {

using nix::Displacement;
using nix::StaticEnv;
using nix::UndefinedVarError;

#define A(X) (dynamic_cast<nixd::nodes::StaticBindable *>(X))

void ExprInt::bindVarsStatic(nix::SymbolTable &Symbols,
                             nix::PosTable &Positions, const StaticEnv &Env) {}

void ExprFloat::bindVarsStatic(nix::SymbolTable &Symbols,
                               nix::PosTable &Positions, const StaticEnv &Env) {
}

void ExprString::bindVarsStatic(nix::SymbolTable &Symbols,
                                nix::PosTable &Positions,
                                const StaticEnv &Env) {}

void ExprPath::bindVarsStatic(nix::SymbolTable &Symbols,
                              nix::PosTable &Positions, const StaticEnv &Env) {}

void ExprVar::bindVarsStatic(nix::SymbolTable &Symbols,
                             nix::PosTable &Positions, const StaticEnv &Env) {

  /* Check whether the variable appears in the environment.  If so,
     set its level and displacement. */
  const StaticEnv *CurEnv;
  nix::Level Level;
  std::optional<nix::Level> WithLevel;
  for (CurEnv = &Env, Level = 0; CurEnv; CurEnv = CurEnv->up, Level++) {
    if (CurEnv->isWith) {
      if (!WithLevel)
        WithLevel = Level;
    } else {
      auto It = CurEnv->find(name);
      if (It != CurEnv->vars.end()) {
        fromWith = false;
        this->level = Level;
        displ = It->second;
        return;
      }
    }
  }

  /* Otherwise, the variable must be obtained from the nearest
     enclosing `with'.  If there is no `with', then we can issue an
     "undefined variable" error now. */
  if (!WithLevel)
    throw UndefinedVarError(
        {.msg = hintfmt("undefined variable '%1%'", Symbols[name]),
         .errPos = Positions[pos]});
  fromWith = true;
  this->level = *WithLevel;
}

void ExprSelect::bindVarsStatic(nix::SymbolTable &Symbols,
                                nix::PosTable &Positions,
                                const StaticEnv &Env) {
  A(e)->bindVarsStatic(Symbols, Positions, Env);
  if (def)
    A(def)->bindVarsStatic(Symbols, Positions, Env);
  for (auto &Name : attrPath)
    if (!Name.symbol)
      A(Name.expr)->bindVarsStatic(Symbols, Positions, Env);
}

void ExprOpHasAttr::bindVarsStatic(nix::SymbolTable &Symbols,
                                   nix::PosTable &Positions,
                                   const StaticEnv &Env) {

  A(e)->bindVarsStatic(Symbols, Positions, Env);
  for (auto &Name : attrPath)
    if (!Name.symbol)
      A(Name.expr)->bindVarsStatic(Symbols, Positions, Env);
}

void ExprAttrs::bindVarsStatic(nix::SymbolTable &Symbols,
                               nix::PosTable &Positions, const StaticEnv &Env) {

  if (recursive) {
    auto NewEnv = StaticEnv(false, &Env, recursive ? attrs.size() : 0);

    Displacement Displ = 0;
    for (auto &I : attrs)
      NewEnv.vars.emplace_back(I.first, I.second.displ = Displ++);

    // No need to sort newEnv since attrs is in sorted order.

    for (auto &I : attrs)
      A(I.second.e)
          ->bindVarsStatic(Symbols, Positions,
                           I.second.inherited ? Env : NewEnv);

    for (auto &I : dynamicAttrs) {
      A(I.nameExpr)->bindVarsStatic(Symbols, Positions, NewEnv);
      A(I.valueExpr)->bindVarsStatic(Symbols, Positions, NewEnv);
    }
  } else {
    for (auto &I : attrs)
      A(I.second.e)->bindVarsStatic(Symbols, Positions, Env);

    for (auto &I : dynamicAttrs) {
      A(I.nameExpr)->bindVarsStatic(Symbols, Positions, Env);
      A(I.valueExpr)->bindVarsStatic(Symbols, Positions, Env);
    }
  }
}

void ExprList::bindVarsStatic(nix::SymbolTable &Symbols,
                              nix::PosTable &Positions, const StaticEnv &Env) {

  for (auto &I : elems)
    A(I)->bindVarsStatic(Symbols, Positions, Env);
}

void ExprLambda::bindVarsStatic(nix::SymbolTable &Symbols,
                                nix::PosTable &Positions,
                                const StaticEnv &Env) {

  auto NewEnv =
      StaticEnv(false, &Env,
                (hasFormals() ? formals->formals.size() : 0) + (!arg ? 0 : 1));

  Displacement Displ = 0;

  if (arg)
    NewEnv.vars.emplace_back(arg, Displ++);

  if (hasFormals()) {
    for (auto &I : formals->formals)
      NewEnv.vars.emplace_back(I.name, Displ++);

    NewEnv.sort();

    for (auto &I : formals->formals)
      if (I.def)
        A(I.def)->bindVarsStatic(Symbols, Positions, NewEnv);
  }

  A(body)->bindVarsStatic(Symbols, Positions, NewEnv);
}

void ExprCall::bindVarsStatic(nix::SymbolTable &Symbols,
                              nix::PosTable &Positions, const StaticEnv &Env) {

  A(fun)->bindVarsStatic(Symbols, Positions, Env);
  for (auto *E : args)
    A(E)->bindVarsStatic(Symbols, Positions, Env);
}

void ExprLet::bindVarsStatic(nix::SymbolTable &Symbols,
                             nix::PosTable &Positions, const StaticEnv &Env) {

  auto NewEnv = StaticEnv(false, &Env, attrs->attrs.size());

  Displacement Displ = 0;
  for (auto &I : attrs->attrs)
    NewEnv.vars.emplace_back(I.first, I.second.displ = Displ++);

  // No need to sort newEnv since attrs->attrs is in sorted order.

  for (auto &I : attrs->attrs)
    A(I.second.e)
        ->bindVarsStatic(Symbols, Positions, I.second.inherited ? Env : NewEnv);

  A(body)->bindVarsStatic(Symbols, Positions, NewEnv);
}

void ExprWith::bindVarsStatic(nix::SymbolTable &Symbols,
                              nix::PosTable &Positions, const StaticEnv &Env) {

  /* Does this `with' have an enclosing `with'?  If so, record its
     level so that `lookupVar' can look up variables in the previous
     `with' if this one doesn't contain the desired attribute. */
  const StaticEnv *CurEnv;
  nix::Level Level;
  prevWith = 0;
  for (CurEnv = &Env, Level = 1; CurEnv; CurEnv = CurEnv->up, Level++)
    if (CurEnv->isWith) {
      prevWith = Level;
      break;
    }

  A(attrs)->bindVarsStatic(Symbols, Positions, Env);
  auto NewEnv = StaticEnv(true, &Env);
  A(body)->bindVarsStatic(Symbols, Positions, NewEnv);
}

void ExprIf::bindVarsStatic(nix::SymbolTable &Symbols, nix::PosTable &Positions,
                            const StaticEnv &Env) {

  A(cond)->bindVarsStatic(Symbols, Positions, Env);
  A(then)->bindVarsStatic(Symbols, Positions, Env);
  A(else_)->bindVarsStatic(Symbols, Positions, Env);
}

void ExprAssert::bindVarsStatic(nix::SymbolTable &Symbols,
                                nix::PosTable &Positions,
                                const StaticEnv &Env) {

  A(cond)->bindVarsStatic(Symbols, Positions, Env);
  A(body)->bindVarsStatic(Symbols, Positions, Env);
}

void ExprOpNot::bindVarsStatic(nix::SymbolTable &Symbols,
                               nix::PosTable &Positions, const StaticEnv &Env) {
  A(e)->bindVarsStatic(Symbols, Positions, Env);
}

void ExprConcatStrings::bindVarsStatic(nix::SymbolTable &Symbols,
                                       nix::PosTable &Positions,
                                       const StaticEnv &Env) {

  for (auto &I : *this->es)
    A(I.second)->bindVarsStatic(Symbols, Positions, Env);
}

void ExprPos::bindVarsStatic(nix::SymbolTable &Symbols,
                             nix::PosTable &Positions, const StaticEnv &Env) {}

#define BIN_OP(OP, S)                                                          \
  void OP::bindVarsStatic(nix::SymbolTable &Symbols, nix::PosTable &Positions, \
                          const StaticEnv &Env) {                              \
    A(e1)->bindVarsStatic(Symbols, Positions, Env);                            \
    A(e2)->bindVarsStatic(Symbols, Positions, Env);                            \
  }
#include "nixd/Expr/BinOps.inc"
#undef BIN_OP

} // namespace nixd::nodes
