/// \file
/// \brief Implementation of flatten nested attribute sets code action.

#include "FlattenAttrs.h"
#include "Utils.h"

#include "../Convert.h"

#include <nixf/Basic/Nodes/Attrs.h>

#include <algorithm>

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

  // Parser recovery can leave missing delimiters. Only rewrite complete
  // bindings, so an inner delimiter cannot be mistaken for the outer one.
  const auto CompleteBinding = [&](const nixf::Binding &B) {
    return B.eq() && B.value() &&
           B.rCur().offset() > B.value()->rCur().offset() &&
           B.src(Src).back() == ';';
  };
  if (!CompleteBinding(Bind))
    return;

  const size_t Open = NestedAttrs.lCur().offset();
  const size_t Close = NestedAttrs.rCur().offset() - 1;
  if (Src[Open] != '{' || Src[Close] != '}')
    return;

  for (const auto &Child : NestedBinds->bindings()) {
    const auto &Inner = static_cast<const nixf::Binding &>(*Child);
    if (!CompleteBinding(Inner) || Inner.rCur().offset() > Close)
      return;
  }

  // Copy names without the trivia between path segments. That trivia belongs
  // to the outer binding and must survive once, not once per nested binding.
  std::string Prefix;
  for (const auto &Name : Bind.path().names()) {
    Prefix += Name->src(Src);
    Prefix += ".";
  }

  std::string NewText;
  const auto AppendSource = [&](size_t Begin, size_t End) {
    NewText += Src.substr(Begin, End - Begin);
  };

  // Remove only the outer path's tokens, retaining comments between them.
  auto PathTokens = Bind.path().children();
  std::sort(PathTokens.begin(), PathTokens.end(),
            [](const nixf::Node *L, const nixf::Node *R) {
              return L->lCur().offset() < R->lCur().offset();
            });
  size_t Pos = Bind.lCur().offset();
  for (const auto *Token : PathTokens) {
    AppendSource(Pos, Token->lCur().offset());
    Pos = Token->rCur().offset();
  }
  AppendSource(Pos, Bind.eq()->lCur().offset());
  AppendSource(Bind.eq()->rCur().offset(), Open);

  // Preserve the entire body, including gaps between bindings and the original
  // values. In particular, do not reindent multiline strings or line comments.
  Pos = Open + 1;
  for (const auto &Child : NestedBinds->bindings()) {
    AppendSource(Pos, Child->lCur().offset());
    NewText += Prefix;
    Pos = Child->lCur().offset();
  }
  AppendSource(Pos, Close);
  AppendSource(Close + 1, Bind.rCur().offset() - 1);

  Actions.emplace_back(createSingleEditAction(
      "Flatten nested attribute set",
      lspserver::CodeAction::REFACTOR_REWRITE_KIND, FileURI,
      toLSPRange(Src, Bind.range()), std::move(NewText)));
}

} // namespace nixd
