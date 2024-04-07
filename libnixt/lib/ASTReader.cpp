#include "ASTReader.h"

#include "nixt/Deserialize.h"

#include <nixbc/FileHeader.h>
#include <nixbc/Nodes.h>
#include <nixbc/Origin.h>
#include <nixbc/Type.h>

#include <bc/Read.h>

#include <nix/eval.hh>
#include <nix/fs-input-accessor.hh>
#include <nix/nixexpr.hh>

namespace nixt {

using namespace nixbc;
nix::Expr *ASTDeserializer::eatHookable(std::string_view &Data) {
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wimplicit-fallthrough"

#define HOOKEXPR(TYPE, EXPR) HookExpr##TYPE(EXPR, VMap, EMap, Hdr.Handle)
  auto Hdr = eat<NodeHeader>(Data);
  nix::PosIdx Pos = Ctx.PTable.add(Ctx.Origin, Hdr.BeginLine, Hdr.BeginColumn);
  nix::Expr *NewExpr = nullptr;
  switch (Hdr.Kind) {
  case EK_Null:
    return nullptr;
  case EK_Int:
    NewExpr = new HOOKEXPR(Int, eatExprInt(Data));
    break;
  case EK_Float:
    NewExpr = new HOOKEXPR(Float, eatExprFloat(Data));
    break;
  case EK_String: {
    auto S = bc::eat<std::string>(Data);
    NewExpr = new HOOKEXPR(String, nix::ExprString(std::move(S)));
    break;
  }
  case EK_Path:
    NewExpr = new HOOKEXPR(Path, eatExprPath(Data));
    break;
  case EK_Var: {
    NewExpr = new HOOKEXPR(Var, eatExprVar(Pos, Data));
    break;
  }
  case EK_Select:
    NewExpr = new HOOKEXPR(Select, eatExprSelect(Pos, Data));
    break;
  case EK_OpHasAttr:
    NewExpr = new HOOKEXPR(OpHasAttr, eatExprOpHasAttr(Data));
    break;
  case EK_Attrs:
    NewExpr = new HOOKEXPR(Attrs, eatExprAttrs(Pos, Data));
    break;
  case EK_List:
    NewExpr = new HOOKEXPR(List, eatExprList(Data));
    break;
  case EK_Lambda:
    NewExpr = new HOOKEXPR(Lambda, eatExprLambnda(Pos, Data));
    break;
  case EK_Call:
    NewExpr = new HOOKEXPR(Call, eatExprCall(Pos, Data));
    break;
  case EK_Let:
    NewExpr = new HOOKEXPR(Let, eatExprLet(Data));
    break;
  case EK_With:
    NewExpr = new HOOKEXPR(With, eatExprWith(Pos, Data));
    break;
  case EK_If:
    NewExpr = new HOOKEXPR(If, eatExprIf(Pos, Data));
    break;
  case EK_Assert:
    NewExpr = new HOOKEXPR(Assert, eatExprAssert(Pos, Data));
    break;
  case EK_ConcatStrings:
    NewExpr = new HOOKEXPR(ConcatStrings, eatExprConcatStrings(Pos, Data));
    break;
  case EK_OpNot:
    NewExpr = new HOOKEXPR(OpNot, eatExprOpNot(Data));
    break;
#define BinOp(OP, SYM)                                                         \
  case EK_##OP:                                                                \
    NewExpr = new HOOKEXPR(OP, eatExpr##OP(Pos, Data));                        \
    break;
#include "nixt/BinOps.inc"
#undef BinOp
  default:
    break;
  }
#pragma GCC diagnostic pop

  assert(NewExpr && "Unknown hookable kind!");
  Pool.record(NewExpr);
  return NewExpr;
}

nix::Symbol ASTDeserializer::eatSymbol(std::string_view &Data) {
  return Ctx.STable.create(eat<std::string>(Data));
}

nix::PosIdx ASTDeserializer::eatPos(std::string_view &Data) {
  auto Line = eat<uint32_t>(Data);
  auto Column = eat<uint32_t>(Data);
  return Ctx.PTable.add(Ctx.Origin, Line, Column);
}

nix::ExprInt ASTDeserializer::eatExprInt(std::string_view &Data) {
  return {eat<NixInt>(Data)};
}

nix::ExprFloat ASTDeserializer::eatExprFloat(std::string_view &Data) {
  return {eat<NixFloat>(Data)};
}

nix::ExprPath ASTDeserializer::eatExprPath(std::string_view &Data) {
  return {Ctx.RootFS, bc::eat<std::string>(Data)};
}

nix::ExprVar ASTDeserializer::eatExprVar(nix::PosIdx Pos,
                                         std::string_view &Data) {
  auto Name = eat<std::string>(Data);
  nix::Symbol Sym = Ctx.STable.create(Name);
  return {Pos, Sym};
}

nix::AttrPath ASTDeserializer::eatAttrPath(std::string_view &Data) {
  auto Size = eat<std::size_t>(Data);

  nix::AttrPath Result;
  for (std::size_t I = 0; I < Size; I++) {
    // It could be a symbol, or an expression.
    // The convention is, if it is a symbol, read an "ExprString"
    // Otherwise interpret it as an expression (needs to be evaluated).
    nix::Expr *Expr = eatHookable(Data);
    assert(Expr);
    if (const auto *String = dynamic_cast<nix::ExprString *>(Expr)) {
      // Create a symbol
      Result.emplace_back(Ctx.STable.create(String->s));
    } else {
      Result.emplace_back(Expr);
    }
  }

  return Result;
}

nix::ExprSelect ASTDeserializer::eatExprSelect(nix::PosIdx Pos,
                                               std::string_view &Data) {
  nix::Expr *E = eatHookable(Data);
  assert(E);
  auto AttrPath = eatAttrPath(Data);
  nix::Expr *Default = /*nullable*/ eatHookable(Data);
  return {Pos, E, std::move(AttrPath), Default};
}

nix::ExprOpHasAttr ASTDeserializer::eatExprOpHasAttr(std::string_view &Data) {
  nix::Expr *E = eatHookable(Data);
  assert(E);
  auto AttrPath = eatAttrPath(Data);
  return {E, std::move(AttrPath)};
}

nix::ExprAttrs ASTDeserializer::eatExprAttrs(nix::PosIdx Pos,
                                             std::string_view &Data) {
  nix::ExprAttrs Result(Pos);

  Result.recursive = eat<bool>(Data);

  auto NStaticDefs = eat<std::size_t>(Data);
  for (std::size_t I = 0; I < NStaticDefs; I++) {
    // Read static attr defs
    auto Name = eatSymbol(Data);

    // AttrDef
    nix::Expr *E = eatHookable(Data);
    assert(E);
    nix::PosIdx Pos = eatPos(Data);
    auto Inherited = eat<bool>(Data);
    Result.attrs.insert({Name, {E, Pos, Inherited}});
  }

  auto NDynamicDefs = eat<std::size_t>(Data);
  for (std::size_t I = 0; I < NDynamicDefs; I++) {
    nix::Expr *Name = eatHookable(Data);
    nix::Expr *Value = eatHookable(Data);
    assert(Name && Value);
    nix::PosIdx Pos = eatPos(Data);
    Result.dynamicAttrs.emplace_back(Name, Value, Pos);
  }

  return Result;
}

nix::ExprList ASTDeserializer::eatExprList(std::string_view &Data) {
  nix::ExprList Result;

  auto N = eat<std::size_t>(Data);

  for (std::size_t I = 0; I < N; I++) {
    nix::Expr *E = eatHookable(Data);
    assert(E);
    Result.elems.emplace_back(E);
  }

  return Result;
}

nix::ExprLambda ASTDeserializer::eatExprLambnda(nix::PosIdx Pos,
                                                std::string_view &Data) {
  // Does this lambda have a unified argument?
  // a: a + 1
  // ^<-------- this symbol
  std::optional<nix::Symbol> Arg;
  auto HasArg = eat<bool>(Data);

  if (HasArg)
    Arg = eatSymbol(Data);

  // Does this lambda have formal arguments?
  // { a, b }: a + b
  auto HasFormals = eat<bool>(Data);
  nix::Formals *Formals = nullptr;

  if (HasFormals) {
    // Decode the formals
    // FIXME: !! Apparently leaks here.
    Formals = new nix::Formals;
    Formals->ellipsis = eat<bool>(Data);

    auto N = eat<std::size_t>(Data);
    for (std::size_t I = 0; I < N; I++) {
      nix::PosIdx Pos = eatPos(Data);
      nix::Symbol Name = eatSymbol(Data);
      // explicitly nullable, nullptr means there is no default
      nix::Expr *Def = /*nullable*/ eatHookable(Data);
      Formals->formals.emplace_back(Pos, Name, Def);
    }
  }

  nix::Expr *Body = eatNonNullHookable(Data);

  if (HasArg)
    return {Pos, *Arg, Formals, Body};
  return {Pos, Formals, Body};
}

nix::ExprCall ASTDeserializer::eatExprCall(nix::PosIdx Pos,
                                           std::string_view &Data) {
  nix::Expr *Fn = eatHookable(Data);
  assert(Fn);

  std::vector<nix::Expr *> Args;
  auto N = eat<std::size_t>(Data);
  for (std::size_t I = 0; I < N; I++) {
    nix::Expr *Arg = eatHookable(Data);
    assert(Arg);
    Args.emplace_back(Arg);
  }

  return {Pos, Fn, std::move(Args)};
}

nix::ExprLet ASTDeserializer::eatExprLet(std::string_view &Data) {
  auto *Attrs = dynamic_cast<nix::ExprAttrs *>(eatHookable(Data));
  if (!Attrs)
    Attrs = Pool.record(new nix::ExprAttrs());
  nix::Expr *Body = eatNonNullHookable(Data);
  return {Attrs, Body};
}

nix::ExprWith ASTDeserializer::eatExprWith(nix::PosIdx Pos,
                                           std::string_view &Data) {
  nix::Expr *Attrs = eatNonNullHookable(Data);
  nix::Expr *Body = eatNonNullHookable(Data);

  return {Pos, Attrs, Body};
}

nix::ExprIf ASTDeserializer::eatExprIf(nix::PosIdx Pos,
                                       std::string_view &Data) {
  nix::Expr *Cond = eatNonNullHookable(Data);
  nix::Expr *Then = eatNonNullHookable(Data);
  nix::Expr *Else = eatNonNullHookable(Data);

  return {Pos, Cond, Then, Else};
}

nix::ExprAssert ASTDeserializer::eatExprAssert(nix::PosIdx Pos,
                                               std::string_view &Data) {
  nix::Expr *Cond = eatNonNullHookable(Data);
  nix::Expr *Body = eatNonNullHookable(Data);

  return {Pos, Cond, Body};
}

nix::ExprOpNot ASTDeserializer::eatExprOpNot(std::string_view &Data) {
  return {eatNonNullHookable(Data)};
}

#define BinOp(OP, SYM)                                                         \
  nix::Expr##OP ASTDeserializer::eatExpr##OP(nix::PosIdx Pos,                  \
                                             std::string_view &Data) {         \
    nix::Expr *LHS = eatNonNullHookable(Data);                                 \
    nix::Expr *RHS = eatNonNullHookable(Data);                                 \
    return {Pos, LHS, RHS};                                                    \
  }
#include "nixt/BinOps.inc"
#undef BinOp

nix::ExprConcatStrings
ASTDeserializer::eatExprConcatStrings(nix::PosIdx Pos, std::string_view &Data) {
  bool ForceString = eat<bool>(Data);

  // FIXME: apparently this LEAKS.
  // However in libnixt nix itself leaks many things so who cares this one??
  auto *Expressions = new std::vector<std::pair<nix::PosIdx, nix::Expr *>>;

  auto Size = eat<std::size_t>(Data);

  for (std::size_t I = 0; I < Size; I++) {
    nix::Expr *E = eatHookable(Data);
    assert(E);
    Expressions->emplace_back(E->getPos(), E);
  }

  return {Pos, ForceString, Expressions};
}

DeserializeContext getDeserializeContext(nix::EvalState &State,
                                         std::string_view BasePath,
                                         const nix::Pos::Origin &Origin) {
  auto Base = State.rootPath(nix::CanonPath(BasePath));
  const nix::ref<nix::InputAccessor> RootFS = State.rootFS;

  // Filling the context.
  // These are required for constructing evaluable nix ASTs
  return {.STable = State.symbols,
          .PTable = State.positions,
          .BasePath = Base,
          .RootFS = RootFS,
          .Origin = Origin};
}

nix::Expr *deserializeHookable(std::string_view &Data, DeserializeContext &Ctx,
                               PtrPool<nix::Expr> &Pool, ValueMap &VMap,
                               EnvMap &EMap) {
  // Deserialize "Hookable"
  ASTDeserializer D(Ctx, Pool, VMap, EMap);
  return D.eatHookable(Data);
}

} // namespace nixt
