#include "nixd/Sema/Lowering.h"
#include "nixd/Sema/EvalContext.h"
#include "nixd/Syntax/Diagnostic.h"
#include "nixd/Syntax/Nodes.h"

namespace nixd {

nix::Formal Lowering::lowerFormal(EvalContext &Ctx,
                                  const syntax::Formal &Formal) {
  auto *Def = Lowering::lower(Ctx, Formal.Default);
  nix::Formal F;

  F.pos = Formal.Range.Begin;
  F.name = Formal.ID->Symbol;
  F.def = Def;
  return F;
}

nix::ExprLambda *Lowering::lowerFunction(EvalContext &Ctx,
                                         syntax::Function *Fn) {
  // Implementation note:
  // The official parser does this in the semantic action, and we deferred it
  // here, as a part of the progressive lowering process.
  // We should be consistant with the official implementation, not strictly
  // required.

  // @toFormals
  // Link: src/libexpr/parser.y#L156 2a52ec4e928c254338a612a6b40355512298ef38

  if (!Fn)
    return nullptr;

  auto *Formals = Fn->FunctionFormals;

  nix::Formals *NixFormals = nullptr;

  if (Formals) {
    std::sort(Formals->Formals.begin(), Formals->Formals.end(),
              [](syntax::Formal *A, syntax::Formal *B) {
                auto APos = A->Range.Begin;
                auto BPos = B->Range.Begin;
                return std::tie(A->ID->Symbol, APos) <
                       std::tie(B->ID->Symbol, BPos);
              });

    // Check if there is a formal duplicated to another, if so, emit a
    // diagnostic expressively to our user
    //
    // We are not using upstream O(n) algorithm because we need to detect all
    // duplicated stuff, instead of the first one.
    std::map<nix::Symbol, const nixd::syntax::Formal *> Names;
    for (const auto *Formal : Formals->Formals) {
      auto Sym = Formal->ID->Symbol;
      if (Names.contains(Sym)) {
        // We have seen this symbol before, so this formal is duplicated
        const syntax::Formal *Previous = Names[Sym];
        syntax::Diagnostic Diag;
        Diag.Kind = syntax::Diagnostic::Error;
        Diag.Msg = "duplicated function formal declaration";
        Diag.Range = Formal->Range;

        syntax::Note PrevDef;
        PrevDef.Msg = "previously declared here";
        PrevDef.Range = Previous->Range;
        Diag.Notes.emplace_back(std::move(PrevDef));

        Diags.emplace_back(std::move(Diag));
      } else {
        Names[Sym] = Formal;
      }
    }

    // Check if the function argument is duplicated with the formal
    if (Fn->Arg) {
      auto ArgSym = Fn->Arg->Symbol;
      if (Names.contains(ArgSym)) {
        const syntax::Formal *Previous = Names[ArgSym];
        syntax::Diagnostic Diag;
        Diag.Kind = syntax::Diagnostic::Error;
        Diag.Msg = "function argument duplicated to a function formal";
        Diag.Range = Fn->Arg->Range;

        syntax::Note PrevDef;
        PrevDef.Msg = "duplicated to this formal";
        PrevDef.Range = Previous->Range;
        Diag.Notes.emplace_back(std::move(PrevDef));

        Diags.emplace_back(std::move(Diag));
      }
    }
    // Construct nix::Formals, from ours
    auto *NixFormals = Ctx.FormalsPool.record(new nix::Formals);
    NixFormals->ellipsis = Formals->Ellipsis;
    for (auto [_, Formal] : Names) {
      nix::Formal F = lowerFormal(Ctx, *Formal);
      NixFormals->formals.emplace_back(F);
    }
  }

  auto *Body = lower(Ctx, Fn->Body);

  nix::ExprLambda *NewLambda;
  if (Fn->Arg) {
    NewLambda =
        new nix::ExprLambda(Fn->Range.Begin, Fn->Arg->Symbol, NixFormals, Body);
  } else {
    NewLambda = new nix::ExprLambda(Fn->Range.Begin, NixFormals, Body);
  }
  Ctx.Pool.record(NewLambda);

  return NewLambda;
}

nix::Expr *Lowering::lower(EvalContext &Ctx, nixd::syntax::Node *Root) {
  if (!Root)
    return nullptr;

  using syntax::Node;

  switch (Root->getKind()) {
  case Node::NK_Function: {
    auto *Fn = dynamic_cast<syntax::Function *>(Root);
    return lowerFunction(Ctx, Fn);
  }
  }

  return nullptr;
}

} // namespace nixd
