/// \file
/// \brief Semantic lowering of AST nodes.
///
/// This file implements the lowering of AST nodes *in place*.
///
/// Syntax AST nodes are lowered to semantic AST nodes. They share part of the
/// nodes actually, for example `ExprInt`.
///
/// The lowering is done in place, so the AST nodes are mutated.
///

#include "Lowering.h"

namespace nixf {

void lower(Node *AST, std::vector<Diagnostic> &Diags) {
  Lowering(Diags).lower(AST);
}

void Lowering::dupAttr(std::string Name, LexerCursorRange Range,
                       LexerCursorRange Prev) {
  auto &Diag = Diags.emplace_back(Diagnostic::DK_DuplicatedAttrName, Range);
  Diag << std::move(Name);
  Diag.note(Note::NK_PrevDeclared, Prev);
}

void Lowering::insertAttr(SemaAttrs &Attr, AttrName &Name,
                          UniqueOrRaw<Evaluable, SemaAttrs> E) {
  using AttrBody = SemaAttrs::AttrBody;
  if (Name.isStatic()) {
    std::map<std::string, AttrBody> &Attrs = Attr.staticAttrs();
    std::string StaticName = Name.staticName();
    if (auto Nested = Attrs.find(StaticName); Nested != Attrs.end()) {
      // TODO: merge two attrset, if applicable.
      const auto &[K, V] = *Nested;
      dupAttr(StaticName, Name.range(), V.name().range());
    } else {
      // Check if the expression actually exists, if it is nullptr, exit early
      //
      // e.g. { a = ; }
      //           ^ nullptr
      //
      // Duplicate checking will be performed on this in-complete attrset,
      // however it will not be placed in the final Sema node.
      if (!E.getRaw())
        return;
      Attrs[StaticName] = AttrBody(/*Inherited=*/false, &Name, std::move(E));
    }
  }
}

SemaAttrs *
Lowering::selectOrCreate(SemaAttrs &Attr,
                         const std::vector<std::unique_ptr<AttrName>> &Path) {
  assert(!Path.empty() && "AttrPath has at least 1 name");
  using DynamicAttr = SemaAttrs::DynamicAttr;
  using AttrBody = SemaAttrs::AttrBody;
  SemaAttrs *Inner = &Attr;
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
      std::map<std::string, AttrBody> &StaticAttrs = Inner->staticAttrs();
      std::string StaticName = Name->staticName();
      if (auto Nested = StaticAttrs.find(StaticName);
          Nested != StaticAttrs.end()) {
        // Find another attr, with the same name.
        const auto &[K, V] = *Nested;
        if (V.inherited() || !V.attrs()) {
          dupAttr(StaticName, Name->range(), V.name().range());
          return nullptr;
        }
        Inner = V.attrs();
      } else {
        // There is no existing one, let's create a new attribute.
        // These attributes are implicitly created, and to match default ctor
        // in C++ nix implementation, they are all non-recursive.
        auto NewNested =
            std::make_unique<SemaAttrs>(Name.get(), /*Recursive=*/false);
        Inner = NewNested.get();
        StaticAttrs[StaticName] = SemaAttrs::AttrBody(
            /*Inherited=*/false, Name.get(), std::move(NewNested));
      }
    } else {
      // Create a dynamic attribute.
      lower(Name.get());
      auto *NameExpr = static_cast<AttrName *>(Name.get());
      std::vector<DynamicAttr> &DynamicAttrs = Inner->dynamicAttrs();
      auto NewNested =
          std::make_unique<SemaAttrs>(Name.get(), /*Recursive=*/false);
      Inner = NewNested.get();
      DynamicAttrs.emplace_back(
          NameExpr, std::unique_ptr<Evaluable>(std::move(NewNested)));
    }
  }
  return Inner;
}

void Lowering::addAttr(SemaAttrs &Attr, const AttrPath &Path,
                       UniqueOrRaw<Evaluable, SemaAttrs> E) {
  // Select until the inner-most attr.
  SemaAttrs *Inner = selectOrCreate(Attr, Path.names());
  if (!Inner)
    return;

  // Insert the attribute.
  const std::unique_ptr<AttrName> &Name = Path.names().back();
  if (!Name)
    return;
  insertAttr(*Inner, *Name, std::move(E));
}

void Lowering::lower(Node *AST) {
  if (!AST)
    return;
  switch (AST->kind()) {
  case Node::NK_ExprAttrs:
    lowerExprAttrs(*static_cast<ExprAttrs *>(AST));
    break;
  default:
    for (auto &Child : AST->children())
      lower(Child);
  }
}

void Lowering::lowerBinds(Binds &B, SemaAttrs &SA) {
  for (const std::unique_ptr<Node> &Bind : B.bindings()) {
    assert(Bind && "Bind is not null");
    switch (Bind->kind()) {
    case Node::NK_Inherit: {
      assert(false && "Not yet implemented");
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

void Lowering::lowerExprAttrs(ExprAttrs &Attrs) {
  auto SA = std::make_unique<SemaAttrs>(&Attrs, Attrs.isRecursive());
  if (Attrs.binds())
    lowerBinds(*Attrs.binds(), *SA);
  Attrs.addSema(std::move(SA));
}

} // namespace nixf
