#pragma once

#include "nixd/Sema/EvalContext.h"
#include "nixd/Syntax/Diagnostic.h"
#include "nixd/Syntax/Nodes.h"

#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>

namespace nixd {

struct Lowering;

class ExprAttrsBuilder {
  // The official parser does this work during the lowering process, via
  // @addAttr in src/libexpr/parser.y

  Lowering &LW;
  nix::ExprAttrs *Result;

  RangeIdx Range;

  bool Recursive;

  /// let ... in ...
  /// It is not allowed to use dynamic binds here, so we want to give diagnostic
  /// to each occurrence.
  bool IsLet;

  /// Nested attributes, we create a new builder for them, and collapse the map
  /// while finishing
  std::map<nix::Symbol, std::unique_ptr<ExprAttrsBuilder>> Nested;

  std::map<syntax::Node *, std::unique_ptr<ExprAttrsBuilder>> DynamicNested;

  // Use a map to detect duplicated fields
  // Not using the map inside nix::ExprAttrs because we want to preserve the
  // range
  std::map<nix::Symbol, const syntax::Node *> Fields;

public:
  ExprAttrsBuilder(Lowering &LW, RangeIdx Range, bool Recursive, bool IsLet);

  void addAttr(const syntax::Node *Attr, const syntax::Node *Body,
               bool Recursive);
  void addAttribute(const syntax::Attribute &A);
  void addBinds(const syntax::Binds &Binds);
  void addAttrSet(const syntax::AttrSet &AS);
  void addInherited(const syntax::InheritedAttribute &IA);
  nix::ExprAttrs *finish();
};

struct Lowering {
  nix::SymbolTable &STable;
  nix::PosTable &PTable;
  std::vector<syntax::Diagnostic> &Diags;
  EvalContext &Ctx;

  nix::Expr *lower(const syntax::Node *Root);
  nix::ExprLambda *lowerFunction(const syntax::Function *Fn);
  nix::Formal lowerFormal(const syntax::Formal &Formal);
  nix::AttrPath lowerAttrPath(const syntax::AttrPath &Path);
  nix::Expr *lowerOp(const syntax::Node *Op);

private:
  constexpr static std::string_view LessThan = "__lessThan";
  constexpr static std::string_view Sub = "__sub";
  constexpr static std::string_view Mul = "__mul";
  constexpr static std::string_view Div = "__div";
  constexpr static std::string_view CurPos = "__curPos";

  nix::Expr *stripIndentation(const syntax::IndStringParts &ISP);

  nix::ExprVar *mkVar(std::string_view Sym) {
    return mkVar(STable.create(Sym));
  }

  nix::ExprVar *mkVar(nix::Symbol Sym) {
    auto *Var = new nix::ExprVar(Sym);
    Ctx.Pool.record(Var);
    return Var;
  };

  nix::ExprOpNot *mkNot(nix::Expr *Body) {
    auto *OpNot = new nix::ExprOpNot(Body);
    Ctx.Pool.record(OpNot);
    return OpNot;
  }

  nix::ExprCall *mkCall(std::string_view FnName, nix::PosIdx P,
                        std::vector<nix::Expr *> Args) {
    nix::ExprVar *Fn = mkVar(FnName);
    auto *Nix = new nix::ExprCall(P, Fn, std::move(Args));
    return Ctx.Pool.record(Nix);
  }

  template <class SyntaxTy>
  nix::Expr *lowerCallOp(std::string_view FnName, const syntax::Node *Op,
                         bool SwapOperands = false, bool Not = false) {
    auto *SOP = dynamic_cast<const SyntaxTy *>(Op);
    assert(SOP && "types are not matching between op pointer & syntax type!");
    nix::Expr *LHS = lower(SOP->LHS);
    nix::Expr *RHS = lower(SOP->RHS);
    if (SwapOperands)
      std::swap(LHS, RHS);
    nix::PosIdx P = SOP->OpRange.Begin;
    nix::ExprCall *Call = mkCall(FnName, P, {LHS, RHS});
    if (Not)
      return mkNot(Call);
    return Call;
  }

  /// The bin-op expression is actually legal in the nix evaluator
  /// OpImpl -> nix::ExprOpImpl
  template <class NixTy, class SyntaxTy>
  NixTy *lowerLegalOp(const syntax::Node *Op) {
    auto *SOP = dynamic_cast<const SyntaxTy *>(Op);
    assert(SOP && "types are not matching between op pointer & syntax type!");
    nix::Expr *LHS = lower(SOP->LHS);
    nix::Expr *RHS = lower(SOP->RHS);
    nix::PosIdx P = SOP->OpRange.Begin;
    auto *Nix = new NixTy(P, LHS, RHS);
    return Ctx.Pool.record(Nix);
  }
};

} // namespace nixd
