/// \file
/// \brief Implementation of pack dotted paths code action.

#include "PackAttrs.h"
#include "Utils.h"

#include "../Convert.h"

#include <nixf/Basic/Nodes/Attrs.h>

#include <optional>

namespace nixd {

namespace {

/// \brief Maximum recursion depth for nested text generation.
/// Prevents stack overflow on maliciously crafted deeply nested inputs.
constexpr size_t MaxNestedDepth = 100;

/// \brief Get the number of sibling bindings sharing the same first path
/// segment. Uses SemaAttrs to count nested attributes for the first segment.
/// Returns 0 if the segment is not found or has conflicts (non-path binding).
size_t getSiblingCount(const nixf::Binding &Bind,
                       const nixf::ExprAttrs &ParentAttrs) {
  const auto &Names = Bind.path().names();
  if (Names.empty() || !Names[0]->isStatic())
    return 0;

  const std::string &FirstSeg = Names[0]->staticName();
  const nixf::SemaAttrs &SA = ParentAttrs.sema();

  auto It = SA.staticAttrs().find(FirstSeg);
  if (It == SA.staticAttrs().end())
    return 0;

  const nixf::Attribute &Attr = It->second;

  // Check if value is a nested ExprAttrs (path was desugared)
  if (!Attr.value() || Attr.value()->kind() != nixf::Node::NK_ExprAttrs)
    return 0; // Non-ExprAttrs value = conflict with non-path binding

  const auto &NestedAttrs = static_cast<const nixf::ExprAttrs &>(*Attr.value());
  const nixf::SemaAttrs &NestedSA = NestedAttrs.sema();

  // Return 0 if there are dynamic attrs (can't safely pack)
  if (!NestedSA.dynamicAttrs().empty())
    return 0;

  return NestedSA.staticAttrs().size();
}

/// \brief Recursively generate nested attribute set text from SemaAttrs.
/// This produces the fully packed/nested form of attributes.
/// \param Depth Current recursion depth (for safety limit)
void generateNestedText(const nixf::SemaAttrs &SA, llvm::StringRef Src,
                        std::string &Out, size_t Depth = 0) {
  // Safety check: prevent stack overflow from deeply nested structures
  if (Depth > MaxNestedDepth) {
    Out += "{ /* max depth exceeded */ }";
    return;
  }

  Out += "{ ";
  bool First = true;
  for (const auto &[Key, Attr] : SA.staticAttrs()) {
    if (!First)
      Out += " ";
    First = false;

    // Output the key, quoting if necessary
    Out += quoteNixAttrKey(Key);
    Out += " = ";

    // Check if value is a nested ExprAttrs that needs recursive generation
    if (Attr.value() && Attr.value()->kind() == nixf::Node::NK_ExprAttrs) {
      const auto &NestedAttrs =
          static_cast<const nixf::ExprAttrs &>(*Attr.value());
      const nixf::SemaAttrs &NestedSA = NestedAttrs.sema();

      // If all nested attrs come from dotted paths (no inherit, no dynamic),
      // we can generate recursively
      if (NestedSA.dynamicAttrs().empty() && !Attr.fromInherit()) {
        generateNestedText(NestedSA, Src, Out, Depth + 1);
      } else {
        // Use original source text
        Out += Attr.value()->src(Src);
      }
    } else if (Attr.value()) {
      Out += Attr.value()->src(Src);
    }
    Out += ";";
  }
  Out += " }";
}

/// \brief Generate shallow nested attribute set text from original bindings.
/// Unlike generateNestedText, this only expands one level and preserves
/// remaining dotted paths as-is by extracting from original source.
/// \param Binds The original Binds node containing all bindings
/// \param FirstSeg The first path segment to match (e.g., "foo" for "foo.bar")
/// \param Src The source text
/// \param Out Output string to append to
void generateShallowNestedText(const nixf::Binds &Binds,
                               const std::string &FirstSeg, llvm::StringRef Src,
                               std::string &Out) {
  Out += "{ ";
  bool First = true;

  for (const auto &Child : Binds.bindings()) {
    if (Child->kind() != nixf::Node::NK_Binding)
      continue;

    const auto &SibBind = static_cast<const nixf::Binding &>(*Child);
    const auto &Names = SibBind.path().names();

    // Only process bindings that match the first segment
    if (Names.empty() || !Names[0]->isStatic() ||
        Names[0]->staticName() != FirstSeg)
      continue;

    if (!First)
      Out += " ";
    First = false;

    if (Names.size() == 1) {
      // Single segment path (e.g., just "foo") - shouldn't happen in bulk pack
      // but handle it gracefully by using value directly
      Out += quoteNixAttrKey(Names[0]->staticName());
      Out += " = ";
      if (SibBind.value()) {
        Out += SibBind.value()->src(Src);
      }
    } else {
      // Multi-segment path - extract rest of path from source
      // e.g., "foo.bar.x = 1" -> "bar.x = 1"
      const nixf::LexerCursor RestStart = Names[1]->range().lCur();
      const nixf::LexerCursor PathEnd = SibBind.path().range().rCur();

      if (PathEnd.offset() >= RestStart.offset()) {
        std::string_view RestPath = Src.substr(
            RestStart.offset(), PathEnd.offset() - RestStart.offset());
        Out += RestPath;
        Out += " = ";
        if (SibBind.value()) {
          Out += SibBind.value()->src(Src);
        }
      }
    }
    Out += ";";
  }
  Out += " }";
}

/// \brief Find all sibling bindings that share the same first path segment.
/// Returns the range covering all such bindings, or nullopt if not applicable.
std::optional<nixf::LexerCursorRange>
findSiblingBindingsRange(const nixf::Binding &Bind, const nixf::Binds &Binds,
                         const std::string &FirstSeg) {
  nixf::LexerCursor Start = Bind.range().lCur();
  nixf::LexerCursor End = Bind.range().rCur();

  for (const auto &Sibling : Binds.bindings()) {
    if (Sibling->kind() != nixf::Node::NK_Binding)
      continue;

    const auto &SibBind = static_cast<const nixf::Binding &>(*Sibling);
    const auto &SibNames = SibBind.path().names();

    if (SibNames.empty() || !SibNames[0]->isStatic())
      continue;

    if (SibNames[0]->staticName() == FirstSeg) {
      // Expand range to include this sibling
      if (SibBind.range().lCur().offset() < Start.offset())
        Start = SibBind.range().lCur();
      if (SibBind.range().rCur().offset() > End.offset())
        End = SibBind.range().rCur();
    }
  }

  return nixf::LexerCursorRange{Start, End};
}

} // namespace

void addPackAttrsAction(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        const std::string &FileURI, llvm::StringRef Src,
                        std::vector<lspserver::CodeAction> &Actions) {
  // Find if we're inside a Binding
  const nixf::Node *BindingNode = PM.upTo(N, nixf::Node::NK_Binding);
  if (!BindingNode)
    return;

  const auto &Bind = static_cast<const nixf::Binding &>(*BindingNode);
  const auto &Names = Bind.path().names();

  // Must have at least 2 path segments (e.g., foo.bar)
  if (Names.size() < 2)
    return;

  // All path segments must be static
  for (const auto &Name : Names) {
    if (!Name->isStatic())
      return;
  }

  // Check parent ExprAttrs is not recursive
  const nixf::Node *BindsNode = PM.query(Bind);
  if (!BindsNode || BindsNode->kind() != nixf::Node::NK_Binds)
    return;

  const nixf::Node *AttrsNode = PM.query(*BindsNode);
  if (!AttrsNode || AttrsNode->kind() != nixf::Node::NK_ExprAttrs)
    return;

  const auto &ParentAttrs = static_cast<const nixf::ExprAttrs &>(*AttrsNode);
  if (ParentAttrs.isRecursive())
    return;

  const std::string &FirstSeg = Names[0]->staticName();
  size_t SiblingCount = getSiblingCount(Bind, ParentAttrs);

  if (SiblingCount == 0)
    return; // Can't pack (dynamic attrs or other conflicts)

  // Helper lambda to generate Pack One action text
  auto GeneratePackOneText = [&]() -> std::string {
    std::string NewText;
    const std::string_view FirstName = Names[0]->src(Src);

    size_t ValueSize = Bind.value() ? Bind.value()->src(Src).size() : 0;
    NewText.reserve(FirstName.size() + Bind.path().src(Src).size() + ValueSize +
                    15);
    NewText += FirstName;
    NewText += " = { ";

    const nixf::LexerCursor RestStart = Names[1]->range().lCur();
    const nixf::LexerCursor RestEnd = Bind.path().range().rCur();

    // Safety check: ensure valid range to prevent integer underflow
    if (RestEnd.offset() < RestStart.offset())
      return "";

    std::string_view RestPath =
        Src.substr(RestStart.offset(), RestEnd.offset() - RestStart.offset());

    NewText += RestPath;
    NewText += " = ";

    if (Bind.value()) {
      NewText += Bind.value()->src(Src);
    }
    NewText += "; };";
    return NewText;
  };

  if (SiblingCount == 1) {
    // Single binding - offer simple pack action
    std::string NewText = GeneratePackOneText();
    if (NewText.empty())
      return;

    Actions.emplace_back(createSingleEditAction(
        "Pack dotted path to nested set",
        lspserver::CodeAction::REFACTOR_REWRITE_KIND, FileURI,
        toLSPRange(Src, Bind.range()), std::move(NewText)));
  } else {
    // Multiple siblings share the prefix - offer Pack One and bulk pack actions
    const nixf::SemaAttrs &SA = ParentAttrs.sema();
    auto It = SA.staticAttrs().find(FirstSeg);
    if (It == SA.staticAttrs().end())
      return;

    const nixf::Attribute &Attr = It->second;
    if (!Attr.value() || Attr.value()->kind() != nixf::Node::NK_ExprAttrs)
      return;

    const auto &NestedAttrs =
        static_cast<const nixf::ExprAttrs &>(*Attr.value());

    // Find the range covering all sibling bindings (needed for bulk actions)
    const auto &ParentBinds = static_cast<const nixf::Binds &>(*BindsNode);
    auto BulkRange = findSiblingBindingsRange(Bind, ParentBinds, FirstSeg);
    if (!BulkRange)
      return;

    // Action 1: Pack One - pack only the current binding
    std::string PackOneText = GeneratePackOneText();
    if (!PackOneText.empty()) {
      Actions.emplace_back(createSingleEditAction(
          "Pack dotted path to nested set",
          lspserver::CodeAction::REFACTOR_REWRITE_KIND, FileURI,
          toLSPRange(Src, Bind.range()), std::move(PackOneText)));
    }

    // Action 2: Shallow Pack All - pack all siblings but only one level deep
    std::string ShallowText;
    ShallowText += quoteNixAttrKey(FirstSeg);
    ShallowText += " = ";
    generateShallowNestedText(ParentBinds, FirstSeg, Src, ShallowText);
    ShallowText += ";";

    Actions.emplace_back(createSingleEditAction(
        "Pack all '" + FirstSeg + "' bindings to nested set",
        lspserver::CodeAction::REFACTOR_REWRITE_KIND, FileURI,
        toLSPRange(Src, *BulkRange), std::move(ShallowText)));

    // Action 3: Recursive Pack All - fully nest all sibling bindings
    std::string RecursiveText;
    RecursiveText += quoteNixAttrKey(FirstSeg);
    RecursiveText += " = ";
    generateNestedText(NestedAttrs.sema(), Src, RecursiveText);
    RecursiveText += ";";

    Actions.emplace_back(createSingleEditAction(
        "Recursively pack all '" + FirstSeg + "' bindings to nested set",
        lspserver::CodeAction::REFACTOR_REWRITE_KIND, FileURI,
        toLSPRange(Src, *BulkRange), std::move(RecursiveText)));
  }
}

} // namespace nixd
