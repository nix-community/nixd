#include "nixd/Sema/Lowering.h"
#include "nixd/Sema/EvalContext.h"
#include "nixd/Syntax/Diagnostic.h"
#include "nixd/Syntax/Nodes.h"

#include <nix/nixexpr.hh>

#include <llvm/Support/FormatVariadic.h>

namespace nixd {

using syntax::Node;

nix::Formal Lowering::lowerFormal(const syntax::Formal &Formal) {
  auto *Def = Lowering::lower(Formal.Default);
  nix::Formal F;

  F.pos = Formal.Range.Begin;
  F.name = Formal.ID->Symbol;
  F.def = Def;
  return F;
}

nix::ExprLambda *Lowering::lowerFunction(syntax::Function *Fn) {
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
      nix::Formal F = lowerFormal(*Formal);
      NixFormals->formals.emplace_back(F);
    }
  }

  auto *Body = lower(Fn->Body);

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

static syntax::Diagnostic mkAttrDupDiag(std::string Name, RangeIdx Range,
                                        RangeIdx Prev) {
  syntax::Diagnostic Diag;
  Diag.Kind = syntax::Diagnostic::Error;
  Diag.Msg = llvm::formatv("duplicated attr `{0}`", Name);
  Diag.Range = Range;

  syntax::Note Note;
  Note.Msg = llvm::formatv("previously defined here");
  Note.Range = Prev;
  Diag.Notes.emplace_back(std::move(Note));
  return Diag;
}

ExprAttrsBuilder::ExprAttrsBuilder(Lowering &LW) : LW(LW) {
  Result = LW.Ctx.Pool.record(new nix::ExprAttrs);
}

nix::ExprAttrs *ExprAttrsBuilder::finish() { return Result; }

void ExprAttrsBuilder::addAttribute(const syntax::Attribute &A) {}

void ExprAttrsBuilder::addInherited(const syntax::InheritedAttribute &IA) {
  if (IA.InheritedAttrs->Names.empty()) {
    syntax::Diagnostic Diag;
    Diag.Kind = syntax::Diagnostic::Warning;
    Diag.Msg = "empty inherit expression";
    Diag.Range = IA.Range;

    LW.Diags.emplace_back(std::move(Diag));
  }

  // inherit (expr) attr1 attr2
  // lowering `(expr)` to nix expression
  nix::Expr *Subject = LW.lower(IA.E);

  for (Node *Name : IA.InheritedAttrs->Names) {

    // Extract nix::Symbol, report error if we encountered dynamic fields
    nix::Symbol Sym;
    switch (Name->getKind()) {
    case Node::NK_Identifier: {
      Sym = dynamic_cast<syntax::Identifier *>(Name)->Symbol;
      break;
    }
    case Node::NK_String: {
      Sym = LW.STable.create(dynamic_cast<syntax::String *>(Name)->S);
      break;
    }
    default: {
      syntax::Diagnostic Diag;
      Diag.Kind = syntax::Diagnostic::Error;
      Diag.Msg = "dynamic attributes are not allowed in inherit";
      Diag.Range = Name->Range;

      LW.Diags.emplace_back(std::move(Diag));
    }
    }

    // Check if it is duplicated
    if (Fields.contains(Sym)) {
      std::string SymStr = LW.STable[Sym];
      auto Diag = mkAttrDupDiag(SymStr, Name->Range, Fields[Sym]->Range);
      LW.Diags.emplace_back(std::move(Diag));
      continue;
    }

    // Insert an AttrDef to the result
    Fields.insert({Sym, Name});
    nix::Expr *AttrBody;
    if (Subject) {
      // inherit (expr) attr1 attr2
      AttrBody = new nix::ExprSelect(Name->Range.Begin, Subject, Sym);
    } else {
      // inherit attr1 attr2 ...
      AttrBody = new nix::ExprVar(Name->Range.Begin, Sym);
    }
    LW.Ctx.Pool.record(AttrBody);
    nix::ExprAttrs::AttrDef Def(AttrBody, Name->Range.Begin, true);
    Result->attrs.insert({Sym, Def});
  }
}

nix::ExprAttrs *Lowering::lowerBinds(const syntax::Binds &Binds) {
  ExprAttrsBuilder Builder(*this);

  for (auto *Attr : Binds.Attributes) {
    switch (Attr->getKind()) {
    case Node::NK_Attribute: {
      Builder.addAttribute(*dynamic_cast<syntax::Attribute *>(Attr));
      break;
    }
    case Node::NK_InheritedAttribute: {
      Builder.addInherited(*dynamic_cast<syntax::InheritedAttribute *>(Attr));
      break;
    }
    default:
      llvm_unreachable("must be A/IA! incorrect parsing rules?");
    }
  }
  return Builder.finish();
}

nix::Expr *Lowering::lower(nixd::syntax::Node *Root) {
  if (!Root)
    return nullptr;

  switch (Root->getKind()) {
  case Node::NK_Function: {
    auto *Fn = dynamic_cast<syntax::Function *>(Root);
    return lowerFunction(Fn);
  }
  case Node::NK_Assert: {
    auto *Assert = dynamic_cast<syntax::Assert *>(Root);
    auto *Cond = lower(Assert->Cond);
    auto *Body = lower(Assert->Body);
    auto *NixAssert =
        Ctx.Pool.record(new nix::ExprAssert(Assert->Range.Begin, Cond, Body));
    return NixAssert;
  }
  case Node::NK_With: {
    auto *With = dynamic_cast<syntax::With *>(Root);
    auto *Attrs = lower(With->Attrs);
    auto *Body = lower(With->Body);
    auto *NixWith =
        Ctx.Pool.record(new nix::ExprWith(With->Range.Begin, Attrs, Body));
    return NixWith;
  }
  case Node::NK_Binds: {
    auto *Binds = dynamic_cast<syntax::Binds *>(Root);
    return lowerBinds(*Binds);
  }
  case Node::NK_AttrSet: {
    auto *AttrSet = dynamic_cast<syntax::AttrSet *>(Root);
    assert(AttrSet->AttrBinds && "null AttrBinds of the AttrSet!");
    nix::ExprAttrs *Binds = lowerBinds(*AttrSet->AttrBinds);
    Binds->recursive = AttrSet->Recursive;
    return Binds;
  }
  }

  return nullptr;
}

} // namespace nixd
