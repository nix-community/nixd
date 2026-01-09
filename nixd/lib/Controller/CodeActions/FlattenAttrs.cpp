/// \file
/// \brief Implementation of flatten nested attribute sets code action.

#include "FlattenAttrs.h"
#include "Utils.h"

#include "../Convert.h"

#include <nixf/Basic/Nodes/Attrs.h>

namespace nixd {

namespace {

/// \brief Check if an ExprAttrs can be flattened (no rec, inherit, dynamic).
/// Returns the Binds node if flattenable, nullptr otherwise.
const nixf::Binds *getFlattenableBinds(const nixf::ExprAttrs &Attrs) {
  // Block if recursive attribute set
  if (Attrs.isRecursive())
    return nullptr;

  const nixf::Binds *B = Attrs.binds();
  if (!B || B->bindings().empty())
    return nullptr;

  // Check all bindings: must be plain Binding nodes (no Inherit)
  // and all attribute names must be static (no dynamic ${} interpolation)
  for (const auto &Child : B->bindings()) {
    if (Child->kind() != nixf::Node::NK_Binding)
      return nullptr; // Inherit node found

    const auto &Bind = static_cast<const nixf::Binding &>(*Child);
    for (const auto &Name : Bind.path().names()) {
      if (!Name->isStatic())
        return nullptr; // Dynamic attribute name
    }
  }

  return B;
}

} // namespace

void addFlattenAttrsAction(const nixf::Node &N,
                           const nixf::ParentMapAnalysis &PM,
                           const std::string &FileURI, llvm::StringRef Src,
                           std::vector<lspserver::CodeAction> &Actions) {
  // Find if we're inside a Binding
  const nixf::Node *BindingNode = PM.upTo(N, nixf::Node::NK_Binding);
  if (!BindingNode)
    return;

  const auto &Bind = static_cast<const nixf::Binding &>(*BindingNode);

  // Check if the binding's value is an ExprAttrs
  if (!Bind.value() || Bind.value()->kind() != nixf::Node::NK_ExprAttrs)
    return;

  const auto &NestedAttrs = static_cast<const nixf::ExprAttrs &>(*Bind.value());

  // Check if flattenable
  const nixf::Binds *NestedBinds = getFlattenableBinds(NestedAttrs);
  if (!NestedBinds)
    return;

  // Check outer path is static too
  for (const auto &Name : Bind.path().names()) {
    if (!Name->isStatic())
      return;
  }

  // Build the flattened text
  std::string NewText;
  const auto &NestedBindings = NestedBinds->bindings();

  // Pre-allocate to reduce reallocations. The +40 accounts for inner path,
  // " = ", value text, and ";". May under-allocate for complex expressions
  // but still reduces reallocations significantly.
  const std::string_view OuterPath = Bind.path().src(Src);
  size_t EstimatedSize = NestedBindings.size() * (OuterPath.size() + 40);
  NewText.reserve(EstimatedSize);

  for (size_t I = 0; I < NestedBindings.size(); ++I) {
    const auto &InnerBind =
        static_cast<const nixf::Binding &>(*NestedBindings[I]);

    // Build path: outer.inner
    const std::string_view InnerPath = InnerBind.path().src(Src);

    NewText += OuterPath;
    NewText += ".";
    NewText += InnerPath;
    NewText += " = ";

    if (InnerBind.value()) {
      NewText += InnerBind.value()->src(Src);
    }
    NewText += ";";

    if (I + 1 < NestedBindings.size())
      NewText += " ";
  }

  Actions.emplace_back(createSingleEditAction(
      "Flatten nested attribute set",
      lspserver::CodeAction::REFACTOR_REWRITE_KIND, FileURI,
      toLSPRange(Src, Bind.range()), std::move(NewText)));
}

} // namespace nixd
