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

#include <utility>

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes.h"
#include "nixf/Basic/Range.h"
#include "nixf/Basic/UniqueOrRaw.h"
#include "nixf/Sema/Lowering.h"

namespace nixf {

void dupAttr(std::string Name, LexerCursorRange Range, LexerCursorRange Prev,
             std::vector<Diagnostic> &Diags) {
  auto &Diag = Diags.emplace_back(Diagnostic::DK_DuplicatedAttrName, Range);
  Diag << std::move(Name);
  Diag.note(Note::NK_PrevDeclared, Prev);
}

/// Insert the binding: `AttrPath = E;` into \p Attr
void addAttr(SemaAttrs *Attr, const AttrPath &AttrPath,
             UniqueOrRaw<Evaluable, SemaAttrs> E,
             std::vector<Diagnostic> &Diags) {
  assert(!AttrPath.names().empty() && "AttrPath has at least 1 name");
  // Firstly perform a lookup to see if the attribute already exists.
  // And do selection if it exists.
  for (std::size_t I = 0; I + 1 < AttrPath.names().size(); I++) {
    const auto &Name = AttrPath.names()[I];
    assert(Attr && "Attr is not null");
    // Name might be nullptr, e.g.
    // { a..b = 1; }
    if (!Name)
      continue;
    if (Name->isStatic()) {
      // If the name is not found, create a new attribute.
      // Otherwise, do a selection.
      std::map<std::string, SemaAttrs::AttrBody> &Attrs = Attr->Attrs;
      std::string StaticName = Name->staticName();
      if (auto Nested = Attrs.find(StaticName); Nested != Attrs.end()) {
        // Do a selection, or report duplicated.
        const auto &[K, V] = *Nested;
        LexerCursorRange Prev = V.name().range();
        if (V.inherited()) {
          dupAttr(StaticName, Name->range(), Prev, Diags);
        } else {
          if (V.attrs())
            Attr = V.attrs();
          else
            dupAttr(StaticName, Name->range(), Prev, Diags);
        }
      } else {
        // There is no existing one, let's create a new attribute.
        auto NewNested = std::make_unique<SemaAttrs>(Name.get());
        Attr = NewNested.get();
        Attrs[StaticName] = SemaAttrs::AttrBody(/*Inherited=*/false, Name.get(),
                                                std::move(NewNested));
      }
    } else {
      // Create a dynamic attribute.
      lower(Name.get(), Diags);
      auto *NameExpr = static_cast<AttrName *>(Name.get());
      auto NewNested = std::make_unique<SemaAttrs>(Name.get());
      SemaAttrs *OldAttr = Attr;
      Attr = NewNested.get();
      SemaAttrs::DynamicAttr NewDynamic(
          NameExpr, std::unique_ptr<Evaluable>(std::move(NewNested)));
      OldAttr->DynamicAttrs.emplace_back(std::move(NewDynamic));
    }
  }
  // Insert the attr.
  const auto &Name = AttrPath.names().back();
  if (!Name)
    return;
  if (Name->isStatic()) {
    std::map<std::string, SemaAttrs::AttrBody> &Attrs = Attr->Attrs;
    std::string StaticName = Name->staticName();
    if (auto Nested = Attrs.find(StaticName); Nested != Attrs.end()) {
      // TODO: merge two attrset, if applicable.
      const auto &[K, V] = *Nested;
      LexerCursorRange Prev = V.name().range();
      dupAttr(StaticName, Name->range(), Prev, Diags);
    } else {
      Attrs[StaticName] =
          SemaAttrs::AttrBody(/*Inherited=*/false, Name.get(), std::move(E));
    }
  }
}

void lowerBinds(Binds &B, std::vector<Diagnostic> &Diags) {
  auto Attr = std::make_unique<SemaAttrs>(&B);
  for (const auto &Bind : B.bindings()) {
    assert(Bind && "Bind is not null");
    switch (Bind->kind()) {
    case Node::NK_Inherit: {
      assert(false && "Not yet implemented");
      break;
    }
    case Node::NK_Binding: {
      auto *B = static_cast<Binding *>(Bind.get());
      addAttr(Attr.get(), B->path(), B->value(), Diags);
      break;
    }
    default:
      assert(false && "Bind should be either Inherit or Binding");
    }
  }
}

void lowerExprAttrs(ExprAttrs &Attrs, std::vector<Diagnostic> &Diags) {
  if (Attrs.binds())
    lowerBinds(*Attrs.binds(), Diags);
}

void lower(Node *AST, std::vector<Diagnostic> &Diags) {
  if (!AST)
    return;
  switch (AST->kind()) {
  case Node::NK_ExprAttrs:
    lowerExprAttrs(*static_cast<ExprAttrs *>(AST), Diags);
    break;
  default:
    for (auto &Child : AST->children())
      lower(Child, Diags);
  }
}

} // namespace nixf
