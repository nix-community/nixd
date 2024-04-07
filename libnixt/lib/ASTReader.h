#pragma once

#include "nixt/Deserialize.h"
#include "nixt/HookExpr.h"
#include "nixt/PtrPool.h"

#include <bc/Read.h>

#include <nix/nixexpr.hh>

namespace nixt {

class ASTDeserializer {
  DeserializeContext &Ctx;
  PtrPool<nix::Expr> &Pool;
  ValueMap &VMap;
  EnvMap &EMap;

public:
  ASTDeserializer(DeserializeContext &Ctx, PtrPool<nix::Expr> &Pool,
                  ValueMap &VMap, EnvMap &EMap)
      : Ctx(Ctx), Pool(Pool), VMap(VMap), EMap(EMap) {}

  nix::Expr *eatHookable(std::string_view &Data);

  nix::Expr *eatNonNullHookable(std::string_view &Data) {
    nix::Expr *E = eatHookable(Data);
    if (E)
      return E;
    return Pool.record(new nix::ExprVar(Ctx.STable.create("null")));
  }

  template <class T> T eat(std::string_view &Data) { return bc::eat<T>(Data); }

  nix::Symbol eatSymbol(std::string_view &Data);

  nix::PosIdx eatPos(std::string_view &Data);

  nix::ExprInt eatExprInt(std::string_view &Data);

  nix::ExprFloat eatExprFloat(std::string_view &Data);

  nix::ExprPath eatExprPath(std::string_view &Data);

  nix::ExprVar eatExprVar(nix::PosIdx Pos, std::string_view &Data);

  nix::AttrPath eatAttrPath(std::string_view &Data);

  nix::ExprSelect eatExprSelect(nix::PosIdx Pos, std::string_view &Data);

  nix::ExprOpHasAttr eatExprOpHasAttr(std::string_view &Data);

  nix::ExprAttrs eatExprAttrs(nix::PosIdx Pos, std::string_view &Data);

  nix::ExprList eatExprList(std::string_view &Data);

  nix::ExprLambda eatExprLambnda(nix::PosIdx Pos, std::string_view &Data);

  nix::ExprCall eatExprCall(nix::PosIdx Pos, std::string_view &Data);

  nix::ExprLet eatExprLet(std::string_view &Data);

  nix::ExprWith eatExprWith(nix::PosIdx Pos, std::string_view &Data);

  nix::ExprIf eatExprIf(nix::PosIdx Pos, std::string_view &Data);

  nix::ExprAssert eatExprAssert(nix::PosIdx Pos, std::string_view &Data);

  nix::ExprOpNot eatExprOpNot(std::string_view &Data);

#define BinOp(OP, SYM)                                                         \
  nix::Expr##OP eatExpr##OP(nix::PosIdx Pos, std::string_view &Data);
#include "nixt/BinOps.inc"
#undef BinOp

  nix::ExprConcatStrings eatExprConcatStrings(nix::PosIdx Pos,
                                              std::string_view &Data);
};

} // namespace nixt
