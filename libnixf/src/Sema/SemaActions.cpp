/// \file
/// \brief Semantic Actions of AST nodes.
///
/// This file implements semantic actions for AST nodes.

#include <memory>

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes/Attrs.h"
#include "nixf/Basic/Nodes/Expr.h"
#include "nixf/Sema/SemaActions.h"

namespace nixf {

void Sema::dupAttr(std::string Name, LexerCursorRange Range,
                   LexerCursorRange Prev) {
  auto &Diag = Diags.emplace_back(Diagnostic::DK_DuplicatedAttrName, Range);
  Diag << std::move(Name);
  Diag.note(Note::NK_PrevDeclared, Prev);
}

void Sema::checkAttrRecursiveForMerge(const ExprAttrs &XAttrs,
                                      const ExprAttrs &YAttrs) {
  bool XAttrsRec = XAttrs.isRecursive();
  bool YAttrsRec = YAttrs.isRecursive();
  if (XAttrsRec == YAttrsRec)
    return;

  // Different "rec" modifier!
  const Misc *Pointer = XAttrsRec ? XAttrs.rec() : YAttrs.rec();
  auto &D = Diags.emplace_back(Diagnostic::DK_MergeDiffRec, Pointer->range());

  auto XRange = XAttrsRec ? XAttrs.rec()->range() : XAttrs.range();
  D.note(Note::NK_ThisRecursive, XRange) << (XAttrsRec ? "" : "non-");

  auto YRange = YAttrsRec ? YAttrs.rec()->range() : YAttrs.range();
  D.note(Note::NK_RecConsider, YRange)
      << /* Marked as ?recursive */ (YAttrsRec ? "" : "non-")
      << /* Considered as ?recursive */ (XAttrsRec ? "" : "non-");
}

void Sema::mergeAttrSets(SemaAttrs &XAttrs, const SemaAttrs &YAttrs) {
  for (const auto &[K, V] : YAttrs.Static) {
    if (XAttrs.Static.contains(K)) {
      // Don't perform recursively merging
      // e.g.
      /*

      {
        p = { x = { y = 1; }; };
              ^<----------------------  this is duplicated!
        p = { x = { z = 1; }; };
                  ^~~~~~~~<-----------  don't merge nested attrs recursively.
      }

      */
      dupAttr(K, V.key().range(), XAttrs.Static.at(K).key().range());
      continue;
    }
    XAttrs.Static.insert({K, V});
  }
  for (const auto &DAttr : YAttrs.Dynamic) {
    XAttrs.Dynamic.emplace_back(DAttr);
  }
}

void Sema::insertAttr(SemaAttrs &SA, std::shared_ptr<AttrName> Name,
                      std::shared_ptr<Expr> E, bool IsInherit) {
  // In this function we accept nullptr "E".
  //
  // e.g. { a = ; }
  //           ^ nullptr
  //
  // Duplicate checking will be performed on this in-complete attrset,
  // however it will not be placed in the final Sema node.
  assert(Name);
  if (!Name->isStatic()) {
    if (E)
      SA.Dynamic.emplace_back(std::move(Name), std::move(E), IsInherit);
    return;
  }
  auto &Attrs = SA.Static;
  std::string StaticName = Name->staticName();
  if (auto Nested = Attrs.find(StaticName); Nested != Attrs.end()) {
    const auto &[K, V] = *Nested;
    if (V.value() && V.value()->kind() == Node::NK_ExprAttrs && E &&
        E->kind() == Node::NK_ExprAttrs) {
      // If this is also an attrset, we want to merge them.
      auto *XAttrSet = static_cast<ExprAttrs *>(V.value());
      auto *YAttrSet = static_cast<ExprAttrs *>(E.get());
      checkAttrRecursiveForMerge(*XAttrSet, *YAttrSet);
      mergeAttrSets(XAttrSet->SA, YAttrSet->SA);
      return;
    }
    dupAttr(StaticName, Name->range(), V.key().range());
    return;
  }
  if (!E)
    return;
  Attrs.insert(
      {StaticName, Attribute(std::move(Name), std::move(E), IsInherit)});
}

SemaAttrs *
Sema::selectOrCreate(SemaAttrs &SA,
                     const std::vector<std::shared_ptr<AttrName>> &Path) {
  assert(!Path.empty() && "AttrPath has at least 1 name");
  SemaAttrs *Inner = &SA;
  // Firstly perform a lookup to see if the attribute already exists.
  // And do selection if it exists.
  for (std::size_t I = 0; I + 1 < Path.size(); I++) {
    const auto &Name = Path[I];
    assert(Inner && "Attr is not null");
    // Name might be nullptr, e.g.
    // { a..b = 1; }
    if (!Name)
      continue;
    if (Name->isStatic()) {
      std::map<std::string, Attribute> &StaticAttrs = Inner->Static;
      const std::string &StaticName = Name->staticName();
      if (auto Nested = StaticAttrs.find(StaticName);
          Nested != StaticAttrs.end()) {
        // Find another attr, with the same name.
        const auto &[K, V] = *Nested;
        if (V.fromInherit() || !V.value() ||
            V.value()->kind() != Node::NK_ExprAttrs) {
          dupAttr(StaticName, Name->range(), V.key().range());
          return nullptr;
        }
        Inner = &static_cast<ExprAttrs *>(V.value())->SA;
      } else {
        // There is no existing one, let's create a new attribute.
        // These attributes are implicitly created, and to match default ctor
        // in C++ nix implementation, they are all non-recursive.
        auto NewNested = std::make_shared<ExprAttrs>(
            Name->range(), nullptr, nullptr, SemaAttrs(/*Recursive=*/nullptr));
        Inner = &NewNested->SA;
        StaticAttrs.insert({StaticName, Attribute(Name, std::move(NewNested),
                                                  /*FromInherit=*/false)});
      }
    } else {
      // Create a dynamic attribute.
      std::vector<Attribute> &DynamicAttrs = Inner->Dynamic;
      auto NewNested = std::make_shared<ExprAttrs>(
          Name->range(), nullptr, nullptr, SemaAttrs(/*Recursive=*/nullptr));
      Inner = &NewNested->SA;
      DynamicAttrs.emplace_back(Name,
                                std::shared_ptr<Expr>(std::move(NewNested)),
                                /*FromInherit=*/false);
    }
  }
  return Inner;
}

void Sema::addAttr(SemaAttrs &Attr, const AttrPath &Path,
                   std::shared_ptr<Expr> E) {
  // Select until the inner-most attr.
  SemaAttrs *Inner = selectOrCreate(Attr, Path.names());
  if (!Inner)
    return;

  // Insert the attribute.
  std::shared_ptr<AttrName> Name = Path.names().back();
  if (!Name)
    return;
  insertAttr(*Inner, std::move(Name), std::move(E), false);
}

void Sema::removeFormal(Fix &F, const FormalVector::const_iterator &Rm,
                        const FormalVector &FV) {
  const Formal &Fm = **Rm;
  F.edit(TextEdit::mkRemoval(Fm.range()));
  // If it is the first formal, remove second formal's comma.
  // { ..., foo } -> { foo, ... }
  if (Rm != FV.begin() || Rm + 1 == FV.end())
    return;

  Formal &SecondF = **(Rm + 1);
  if (SecondF.comma())
    F.edit(TextEdit::mkRemoval(SecondF.comma()->range()));
}

void Sema::checkFormalEllipsis(const FormalVector &FV) {
  if (FV.empty())
    return;

  Formal &LastF = *FV.back().get(); // Last Formal

  for (auto It = FV.begin(); It + 1 != FV.end(); It++) {
    Formal &CurF = **It; // Current Formal
    if (!CurF.isEllipsis())
      continue;

    if (LastF.isEllipsis()) {
      // extra "formal", suggest remove it.
      Diagnostic &D =
          Diags.emplace_back(Diagnostic::DK_FormalExtraEllipsis, CurF.range());
      removeFormal(D.fix("remove `...`"), It, FV);
    } else {
      Diagnostic &D = Diags.emplace_back(Diagnostic::DK_FormalMisplacedEllipsis,
                                         CurF.range());
      Fix &Fx = D.fix("move ellipsis to the tail");
      removeFormal(Fx, It, FV);
      std::string NewText(CurF.src(Src));
      // If current formal does not contain the seperator ", "
      // Insert a new comma to seperate it from the last formal
      if (!CurF.comma())
        NewText = std::string(", ").append(NewText);
      Fx.edit(TextEdit::mkInsertion(LastF.rCur(), std::move(NewText)));
    }
  }
}

void Sema::checkFormalSep(const FormalVector &FV) {
  if (FV.empty())
    return;
  for (auto It = FV.begin(); It != FV.end(); It++) {
    const Formal &F = **It;
    // All formals must begins with "," except the first.
    if (It != FV.begin() && !F.comma()) {
      Diagnostic &D =
          Diags.emplace_back(Diagnostic::DK_FormalMissingComma, F.range());
      D.fix("insert ,").edit(TextEdit::mkInsertion(F.lCur(), ","));
    }
  }
}

void Sema::checkFormalEmpty(const FormalVector &FV) {
  for (const std::shared_ptr<Formal> &FPtr : FV) {
    const Formal &F = *FPtr;
    // Check if the formal is emtpy, e.g.
    // { , }
    //   ^  empty formal
    if (F.comma() && !F.id() && !F.isEllipsis()) {
      Diagnostic &D = Diags.emplace_back(Diagnostic::DK_EmptyFormal, F.range());
      D.fix("remove empty formal").edit(TextEdit::mkRemoval(F.range()));
      D.tag(DiagnosticTag::Faded);
      continue;
    }
  }
}

void Sema::dedupFormal(std::map<std::string, const Formal *> &Dedup,
                       const FormalVector &FV) {
  for (const std::shared_ptr<Formal> &FPtr : FV) {
    const Formal &F = *FPtr;
    if (!F.id())
      continue;
    const Identifier &ID = *F.id();
    if (Dedup.contains(ID.name())) {
      // Report duplicated formals.
      // All warning ranges should be placed at "Identifiers".
      const Formal &DupF = *Dedup[ID.name()];
      Identifier &DupID = *DupF.id();
      Diagnostic &D =
          Diags.emplace_back(Diagnostic::DK_DuplicatedFormal, ID.range());
      D.note(Note::NK_DuplicateFormal, DupID.range());
    } else {
      Dedup[ID.name()] = &F;
    }
  }
}

std::shared_ptr<Formals> Sema::onFormals(LexerCursorRange Range,
                                         FormalVector FV) {
  std::map<std::string, const Formal *> Dedup;
  checkFormalSep(FV);
  checkFormalEllipsis(FV);
  checkFormalEmpty(FV);
  dedupFormal(Dedup, FV);
  return std::make_shared<Formals>(Range, std::move(FV), std::move(Dedup));
}

void Sema::lowerInheritName(SemaAttrs &SA, std::shared_ptr<AttrName> Name,
                            std::shared_ptr<Expr> E) {
  if (!Name)
    return;
  if (!Name->isStatic()) {
    // Not allowed to have dynamic attrname in inherit.
    Diagnostic &D =
        Diags.emplace_back(Diagnostic::DK_DynamicInherit, Name->range());
    D.fix("remove dynamic attrname").edit(TextEdit::mkRemoval(Name->range()));
    D.tag(DiagnosticTag::Striked);
    return;
  }
  // Check duplicated attrname.
  if (SA.Static.contains(Name->staticName())) {
    dupAttr(Name->staticName(), Name->range(),
            SA.Static.at(Name->staticName()).key().range());
    return;
  }
  // Insert the attr.
  std::string StaticName = Name->staticName();
  SA.Static.insert({StaticName, Attribute(std::move(Name), std::move(E),
                                          /*FromInherit=*/true)});
}

void Sema::lowerInherit(SemaAttrs &Attr, const Inherit &Inherit) {
  for (const std::shared_ptr<AttrName> &Name : Inherit.names()) {
    assert(Name);
    std::shared_ptr<Expr> Desugar = desugarInheritExpr(Name, Inherit.expr());
    lowerInheritName(Attr, Name, std::move(Desugar));
  }
}

void Sema::lowerBinds(SemaAttrs &SA, const Binds &B) {
  for (const std::shared_ptr<Node> &Bind : B.bindings()) {
    assert(Bind && "Bind is not null");
    switch (Bind->kind()) {
    case Node::NK_Inherit: {
      auto *N = static_cast<Inherit *>(Bind.get());
      lowerInherit(SA, *N);
      break;
    }
    case Node::NK_Binding: {
      auto *B = static_cast<Binding *>(Bind.get());
      addAttr(SA, B->path(), B->value());
      break;
    }
    default:
      assert(false && "Bind should be either Inherit or Binding");
    }
  }
}

std::shared_ptr<Expr> Sema::desugarInheritExpr(std::shared_ptr<AttrName> Name,
                                               std::shared_ptr<Expr> E) {
  auto Range = Name->range();
  if (!E)
    return std::make_shared<ExprVar>(Range, Name->id());

  auto Path = std::make_shared<AttrPath>(
      Range, std::vector<std::shared_ptr<AttrName>>{std::move(Name)},
      std::vector<std::shared_ptr<Dot>>{});
  return std::make_shared<ExprSelect>(Range, std::move(E), std::move(Path),
                                      nullptr);
}

std::shared_ptr<ExprAttrs> Sema::onExprAttrs(LexerCursorRange Range,
                                             std::shared_ptr<Binds> Binds,
                                             std::shared_ptr<Misc> Rec) {
  SemaAttrs ESA(Rec.get());
  if (Binds)
    lowerBinds(ESA, *Binds);
  return std::make_shared<ExprAttrs>(Range, std::move(Binds), std::move(Rec),
                                     std::move(ESA));
}

std::shared_ptr<LambdaArg> Sema::onLambdaArg(LexerCursorRange Range,
                                             std::shared_ptr<Identifier> ID,
                                             std::shared_ptr<Formals> F) {
  // Check that if lambda arguments duplicated to it's formal

  if (ID && F) {
    if (F->dedup().contains(ID->name())) {
      // Report duplicated.
      Diagnostic &D =
          Diags.emplace_back(Diagnostic::DK_DuplicatedFormalToArg, ID->range());
      D.note(Note::NK_DuplicateFormal, F->dedup().at(ID->name())->range());
    }
  }
  return std::make_shared<LambdaArg>(Range, std::move(ID), std::move(F));
}

} // namespace nixf
