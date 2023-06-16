#include "nixd/AST.h"
#include "nixd/CallbackExpr.h"
#include "nixd/Diagnostic.h"
#include "nixd/Expr.h"
#include "nixd/Position.h"

#include "lspserver/Logger.h"
#include "lspserver/Protocol.h"

#include <nix/canon-path.hh>
#include <nix/nixexpr.hh>

#include <cstddef>
#include <exception>
#include <memory>
#include <stdexcept>

namespace nixd {

void EvalAST::rewriteAST() {
  auto EvalCallback = [this](const nix::Expr *Expr, const nix::EvalState &,
                             const nix::Env &ExprEnv,
                             const nix::Value &ExprValue) {
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

void EvalAST::injectAST(nix::EvalState &State, lspserver::PathRef Path) {
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

const nix::Env *EvalAST::searchUpEnv(const nix::Expr *Expr) const {
  for (;;) {
    if (EnvMap.contains(Expr))
      return EnvMap.at(Expr);
    if (parent(Expr) == Expr)
      break;

    Expr = parent(Expr);
  }
  throw std::out_of_range("No such env associated to ancestors");
}

[[nodiscard]] const nix::Expr *
EvalAST::lookupEnd(lspserver::Position Desired) const {
  struct VTy : RecursiveASTVisitor<VTy> {
    const decltype(Locations) &Loc;
    const decltype(ParseData::end) &End;
    const nix::PosTable &PTable;
    const nix::Expr *R;
    lspserver::Position RStart = {INT_MAX, INT_MAX};
    lspserver::Position REnd = {INT_MIN, INT_MIN};

    const lspserver::Position Desired;

    bool visitExpr(const nix::Expr *E) {
      if (Loc.contains(E)) {
        auto ER = toLSPRange(Range(E, Loc, End, PTable));
        if (ER.end <= Desired) {
          if (REnd < ER.end || (REnd == ER.end && RStart <= ER.start)) {
            // Update the expression.
            R = E;
            RStart = ER.start;
            REnd = ER.end;
          }
        }
      }
      return true;
    }
  } V{.Loc = Locations,
      .End = Data->end,
      .PTable = Data->state.positions,
      .Desired = Desired};

  V.traverseExpr(root());
  return V.R;
}

std::vector<const nix::Expr *>
EvalAST::lookupContain(lspserver::Position Desired) const {
  struct VTy : RecursiveASTVisitor<VTy> {
    const decltype(Locations) &Loc;
    const decltype(ParseData::end) &End;
    const nix::PosTable &PTable;
    std::vector<const nix::Expr *> R;

    const lspserver::Position Desired;

    bool visitExpr(const nix::Expr *E) {
      if (Loc.contains(E)) {
        auto ER = toLSPRange(Range(E, Loc, End, PTable));
        if (ER.contains(Desired))
          R.emplace_back(E);
      }
      return true;
    }
  } V{.Loc = Locations,
      .End = Data->end,
      .PTable = Data->state.positions,
      .Desired = Desired};

  V.traverseExpr(root());
  return V.R;
}

[[nodiscard]] const nix::Expr *
EvalAST::lookupContainMin(lspserver::Position Desired) const {
  struct VTy : RecursiveASTVisitor<VTy> {
    const decltype(Locations) &Loc;
    const decltype(ParseData::end) &End;
    const nix::PosTable &PTable;
    const nix::Expr *R;
    lspserver::Range RR = {{INT_MIN, INT_MIN}, {INT_MAX, INT_MAX}};

    const lspserver::Position Desired;

    bool visitExpr(const nix::Expr *E) {
      if (Loc.contains(E)) {
        auto ER = toLSPRange(Range(E, Loc, End, PTable));
        if (ER.contains(Desired) && RR.contains(ER)) {
          // Update the expression.
          R = E;
          RR = ER;
        }
      }
      return true;
    }

  } V{.Loc = Locations,
      .End = Data->end,
      .PTable = Data->state.positions,
      .Desired = Desired};

  V.traverseExpr(root());
  return V.R;
}

[[nodiscard]] const nix::Expr *
EvalAST::lookupStart(lspserver::Position Desired) const {
  struct VTy : RecursiveASTVisitor<VTy> {
    const decltype(Locations) &Loc;
    const decltype(ParseData::end) &End;
    const nix::PosTable &PTable;
    const nix::Expr *R;
    lspserver::Position RStart = {INT_MAX, INT_MAX};
    lspserver::Position REnd = {INT_MIN, INT_MIN};

    const lspserver::Position Desired;

    bool visitExpr(const nix::Expr *E) {
      if (Loc.contains(E)) {
        auto ER = toLSPRange(Range(E, Loc, End, PTable));
        if (Desired <= ER.start) {
          if (ER.start < RStart || (ER.start == RStart && ER.end <= REnd)) {
            // Update the expression.
            R = E;
            RStart = ER.start;
            REnd = ER.end;
          }
        }
      }
      return true;
    }
  } V{.Loc = Locations,
      .End = Data->end,
      .PTable = Data->state.positions,
      .Desired = Desired};

  V.traverseExpr(root());
  return V.R;
}

} // namespace nixd
