#include "nixd/Sema/Lowering.h"
#include "nixd/Sema/EvalContext.h"
#include "nixd/Syntax/Diagnostic.h"
#include "nixd/Syntax/Nodes.h"
#include "nixd/Syntax/Range.h"

#include <nix/nixexpr.hh>

#include <llvm/Support/FormatVariadic.h>

#include <memory>

namespace nixd {

using syntax::Diagnostic;
using syntax::Node;
using syntax::Note;

nix::Formal Lowering::lowerFormal(const syntax::Formal &Formal) {
  auto *Def = Lowering::lower(Formal.Default);
  nix::Formal F;

  F.pos = Formal.Range.Begin;
  F.name = Formal.ID->Symbol;
  F.def = Def;
  return F;
}

nix::ExprLambda *Lowering::lowerFunction(const syntax::Function *Fn) {
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

ExprAttrsBuilder::ExprAttrsBuilder(Lowering &LW, RangeIdx Range, bool Recursive,
                                   bool IsLet)
    : LW(LW), Range(Range), Recursive(Recursive), IsLet(IsLet) {
  Result = LW.Ctx.Pool.record(new nix::ExprAttrs);
}

nix::ExprAttrs *ExprAttrsBuilder::finish() {
  Result->recursive = Recursive;
  for (const auto &[Sym, Builder] : Nested) {
    auto *NestedAttr = Builder->finish();
    nix::ExprAttrs::AttrDef Def(NestedAttr, NestedAttr->pos, false);
    Result->attrs.insert({Sym, Def});
  }
  for (const auto &[Node, Builder] : DynamicNested) {
    nix::Expr *Name = LW.lower(Node);
    nix::ExprAttrs *Value = Builder->finish();
    nix::ExprAttrs::DynamicAttrDef DDef(Name, Value, Value->pos);
    Result->dynamicAttrs.emplace_back(DDef);
  }
  return Result;
}

void ExprAttrsBuilder::addBinds(const syntax::Binds &Binds) {
  for (Node *Attr : Binds.Attributes) {
    switch (Attr->getKind()) {
    case Node::NK_Attribute:
      addAttribute(*dynamic_cast<syntax::Attribute *>(Attr));
      break;
    case Node::NK_InheritedAttribute:
      addInherited(*dynamic_cast<syntax::InheritedAttribute *>(Attr));
      break;
    default:
      llvm_unreachable("attr in binds must be either A or IA!");
    }
  }
}

static Diagnostic mkRecDiag(RangeIdx ThisRange, RangeIdx MergingRange,
                            bool ThisRec, bool MergningRec) {
  assert(ThisRec != MergningRec);

  Diagnostic Diag;
  Diag.Kind = Diagnostic::Warning;
  Diag.Msg = "merging two attributes with different `rec` modifiers, the "
             "latter will be implicitly ignored";
  Diag.Range = MergingRange;

  Note This;
  This.Range = ThisRange;
  This.Msg = llvm::formatv("this attribute set is{0} recursive",
                           ThisRec ? "" : " not");
  Diag.Notes.emplace_back(std::move(This));

  Note Merging;
  Merging.Range = MergingRange;
  Merging.Msg =
      llvm::formatv("while this attribute set is marked as {0}recursive, it "
                    "will be considered as {1}recursive",
                    MergningRec ? "" : "non-", ThisRec ? "" : "non-");
  Diag.Notes.emplace_back(std::move(Merging));
  return Diag;
}

static Diagnostic mkLetDynamicDiag(RangeIdx Range) {
  Diagnostic Diag;
  Diag.Kind = Diagnostic::Error;
  Diag.Msg = "dynamic attributes are not allowed in let ... in ... expression";
  Diag.Range = Range;

  return Diag;
}

void ExprAttrsBuilder::addAttrSet(const syntax::AttrSet &AS) {
  if (AS.Recursive != Recursive) {
    auto Diag = mkRecDiag(Range, AS.Range, Recursive, AS.Recursive);
    LW.Diags.emplace_back(std::move(Diag));
  }
  assert(AS.AttrBinds && "binds should not be empty! parser error?");
  addBinds(*AS.AttrBinds);
}

void ExprAttrsBuilder::addAttr(const syntax::Node *Attr,
                               const syntax::Node *Body, bool Recursive) {
  if (Recursive != this->Recursive) {
    auto Diag = mkRecDiag(Range, Attr->Range, this->Recursive, Recursive);
    LW.Diags.emplace_back(std::move(Diag));
  }
  switch (Attr->getKind()) {
  case Node::NK_Identifier: {
    const auto *ID = dynamic_cast<const syntax::Identifier *>(Attr);
    nix::Symbol Sym = ID->Symbol;
    if (Fields.contains(Sym)) {
      // duplicated, but if they are two attrset, we can try to merge them.
      if (Body->getKind() != Node::NK_AttrSet || !Nested.contains(Sym)) {
        // the body is not an attribute set, report error.
        auto Diag =
            mkAttrDupDiag(LW.STable[Sym], ID->Range, Fields[Sym]->Range);
        LW.Diags.emplace_back(std::move(Diag));
        return;
      }
      const auto *BodyAttrSet = dynamic_cast<const syntax::AttrSet *>(Body);
      Nested[Sym]->addAttrSet(*BodyAttrSet);
    } else {
      Fields[Sym] = Attr;
      if (Body->getKind() == Node::NK_AttrSet) {
        // If the body is an attrset, we want to defer it's lowering process,
        // because we have chance to merge it latter
        const auto *BodyAttrSet = dynamic_cast<const syntax::AttrSet *>(Body);
        Nested[Sym] = std::make_unique<ExprAttrsBuilder>(
            LW, BodyAttrSet->Range, BodyAttrSet->Recursive, IsLet);
        Nested[Sym]->addBinds(*BodyAttrSet->AttrBinds);
      } else {
        nix::ExprAttrs::AttrDef Def(LW.lower(Body), Attr->Range.Begin);
        Result->attrs.insert({Sym, Def});
      }
    }
    break;
  }
  default: {
    if (IsLet) {
      // reject dynamic attrs in let expression
      LW.Diags.emplace_back(mkLetDynamicDiag(Attr->Range));
      return;
    }
    nix::Expr *NameExpr = LW.lower(Attr);
    nix::Expr *ValueExpr = LW.lower(Body);
    nix::ExprAttrs::DynamicAttrDef DDef(NameExpr, ValueExpr, Attr->Range.Begin);
    Result->dynamicAttrs.emplace_back(DDef);
  }
  }
}

void ExprAttrsBuilder::addAttribute(const syntax::Attribute &A) {
  assert(!A.Path->Names.empty() && "attrpath should not be empty!");

  // The targeting attribute builder we are actually inserting
  // we will select the attrpath (`a.b.c`) and assign it to this variable
  // left only one nix::Symbol or nix::Expr*, insert to it.
  ExprAttrsBuilder *S = this;

  // select attrpath, without the last one.
  for (auto I = A.Path->Names.begin(); I + 1 != A.Path->Names.end(); I++) {
    Node *Name = *I;

    switch (Name->getKind()) {
    case Node::NK_Identifier: {
      auto *ID = dynamic_cast<syntax::Identifier *>(Name);
      nix::Symbol Sym = ID->Symbol;
      if (S->Fields.contains(Sym)) {
        if (!S->Nested.contains(Sym)) {
          // contains a field but the body is not an attr, then we cannot
          // perform merging.
          Diagnostic Diag =
              mkAttrDupDiag(LW.STable[Sym], ID->Range, S->Fields[Sym]->Range);
          LW.Diags.emplace_back(std::move(Diag));
          return;
        }
        // select this symbol, and consider merging it.
        S = S->Nested[Sym].get();
      } else {
        // create a new builder and select to it.
        S->Nested[Sym] = std::make_unique<ExprAttrsBuilder>(
            LW, Name->Range, /*Recursive=*/false, IsLet);
        S->Fields[Sym] = Name;
        S = S->Nested[Sym].get();
      }
      break; // case Node::NK_Identifier:
    }
    default: {
      if (IsLet) {
        // reject dynamic attrs in let expression
        LW.Diags.emplace_back(mkLetDynamicDiag(Name->Range));
        return;
      }
      DynamicNested[Name] = std::make_unique<ExprAttrsBuilder>(
          LW, Name->Range, /*Recursive=*/false, IsLet);
      S = DynamicNested[Name].get();
    }
    }
  }
  // Implictly created attribute set, by attrpath selecting, are always
  // non-recursive
  S->addAttr(A.Path->Names.back(), A.Body,
             /*Recursive=*/S != this ? false : this->Recursive);
}

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

nix::Expr *Lowering::lower(const syntax::Node *Root) {
  if (!Root)
    return nullptr;

  switch (Root->getKind()) {
  case Node::NK_Function: {
    const auto *Fn = dynamic_cast<const syntax::Function *>(Root);
    return lowerFunction(Fn);
  }
  case Node::NK_Assert: {
    const auto *Assert = dynamic_cast<const syntax::Assert *>(Root);
    auto *Cond = lower(Assert->Cond);
    auto *Body = lower(Assert->Body);
    auto *NixAssert =
        Ctx.Pool.record(new nix::ExprAssert(Assert->Range.Begin, Cond, Body));
    return NixAssert;
  }
  case Node::NK_With: {
    const auto *With = dynamic_cast<const syntax::With *>(Root);
    auto *Attrs = lower(With->Attrs);
    auto *Body = lower(With->Body);
    auto *NixWith =
        Ctx.Pool.record(new nix::ExprWith(With->Range.Begin, Attrs, Body));
    return NixWith;
  }
  case Node::NK_AttrSet: {
    const auto *AttrSet = dynamic_cast<const syntax::AttrSet *>(Root);
    assert(AttrSet->AttrBinds && "null AttrBinds of the AttrSet!");
    ExprAttrsBuilder Builder(*this, AttrSet->Range, AttrSet->Recursive,
                             /*IsLet=*/false);
    Builder.addBinds(*AttrSet->AttrBinds);
    return Builder.finish();
  }
  case Node::NK_Int: {
    const auto *Int = dynamic_cast<const syntax::Int *>(Root);
    auto *NixInt = new nix::ExprInt(Int->N);
    Ctx.Pool.record(NixInt);
    return NixInt;
  }
  case Node::NK_Let: {
    const auto *Let = dynamic_cast<const syntax::Let *>(Root);
    ExprAttrsBuilder Builder(*this, Let->Binds->Range, /*Recursive=*/true,
                             /*IsLet=*/true);
    Builder.addBinds(*Let->Binds);
    nix::ExprAttrs *Attrs = Builder.finish();
    nix::Expr *Body = lower(Let->Body);

    // special work for let expression, the expression there is implictly
    // recursive, and we should not mark `rec` to the evaluator, because the
    // official parser does not do this also.
    Attrs->recursive = false;

    auto *Ret = new nix::ExprLet(Attrs, Body);
    return Ret;
  }
  case Node::NK_If: {
    const auto *If = dynamic_cast<const syntax::If *>(Root);
    auto *Cond = lower(If->Cond);
    auto *Then = lower(If->Then);
    auto *Else = lower(If->Else);
    auto *NixIf =
        Ctx.Pool.record(new nix::ExprIf(If->Range.Begin, Cond, Then, Else));
    return NixIf;
  }
  }

  return nullptr;
}

} // namespace nixd
