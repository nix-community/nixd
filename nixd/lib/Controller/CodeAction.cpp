/// \file
/// \brief Implementation of [Code Action].
/// [Code Action]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_codeAction

#include "CheckReturn.h"
#include "Convert.h"

#include "nixd/Controller/Controller.h"

#include <nixf/Basic/Nodes/Attrs.h>
#include <nixf/Sema/ParentMap.h>

#include <boost/asio/post.hpp>
#include <llvm/Support/JSON.h>
#include <lspserver/SourceCode.h>

#include <iomanip>
#include <optional>
#include <set>
#include <sstream>

namespace nixd {

using namespace llvm::json;
using namespace lspserver;

namespace {

/// \brief Create a CodeAction with a single text edit.
CodeAction createSingleEditAction(const std::string &Title,
                                  llvm::StringLiteral Kind,
                                  const std::string &FileURI,
                                  const lspserver::Range &EditRange,
                                  std::string NewText) {
  std::vector<TextEdit> Edits;
  Edits.emplace_back(
      TextEdit{.range = EditRange, .newText = std::move(NewText)});
  using Changes = std::map<std::string, std::vector<TextEdit>>;
  WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};
  return CodeAction{
      .title = Title,
      .kind = std::string(Kind),
      .edit = std::move(WE),
  };
}

/// \brief Check if a string is a valid Nix identifier that can be unquoted.
/// Valid identifiers: start with letter or underscore, contain letters,
/// digits, underscores, hyphens, or single quotes. Must not be a keyword.
bool isValidNixIdentifier(const std::string &S) {
  if (S.empty())
    return false;

  // Check first character: must be letter or underscore
  char First = S[0];
  if (!((First >= 'a' && First <= 'z') || (First >= 'A' && First <= 'Z') ||
        First == '_'))
    return false;

  // Check remaining characters
  for (size_t I = 1; I < S.size(); ++I) {
    char C = S[I];
    if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') ||
          (C >= '0' && C <= '9') || C == '_' || C == '-' || C == '\''))
      return false;
  }

  // Check against Nix keywords and reserved literals
  static const std::set<std::string> Keywords = {
      "if",  "then",    "else", "assert", "with",  "let", "in",
      "rec", "inherit", "or",   "true",   "false", "null"};
  return Keywords.find(S) == Keywords.end();
}

/// \brief Escape special characters for Nix double-quoted string literals.
/// Escapes: " \ ${ \n \r \t (per Nix Reference Manual)
std::string escapeNixString(llvm::StringRef S) {
  std::string Result;
  Result.reserve(S.size() + S.size() / 4 + 2);
  for (size_t I = 0; I < S.size(); ++I) {
    char C = S[I];
    switch (C) {
    case '"':
      Result += "\\\"";
      break;
    case '\\':
      Result += "\\\\";
      break;
    case '\n':
      Result += "\\n";
      break;
    case '\r':
      Result += "\\r";
      break;
    case '\t':
      Result += "\\t";
      break;
    case '$':
      // Only escape ${ to prevent interpolation
      if (I + 1 < S.size() && S[I + 1] == '{') {
        Result += "\\${";
        ++I; // Skip the '{'
      } else {
        Result += C;
      }
      break;
    default:
      Result += C;
    }
  }
  return Result;
}

/// \brief Quote and escape a Nix attribute key if necessary.
/// Returns the key as-is if it's a valid identifier, otherwise quotes and
/// escapes special characters using escapeNixString().
std::string quoteNixAttrKey(const std::string &Key) {
  if (isValidNixIdentifier(Key))
    return Key;

  return "\"" + escapeNixString(Key) + "\"";
}

/// \brief Maximum recursion depth for JSON to Nix conversion.
constexpr size_t MaxJsonDepth = 100;

/// \brief Maximum array/object width for JSON to Nix conversion.
constexpr size_t MaxJsonWidth = 10000;

/// \brief Convert a JSON value to Nix expression syntax.
/// \param V The JSON value to convert
/// \param Indent Current indentation level (for pretty-printing)
/// \param Depth Current recursion depth (safety limit)
/// \return Nix expression string, or empty string on error
std::string jsonToNix(const llvm::json::Value &V, size_t Indent = 0,
                      size_t Depth = 0) {
  if (Depth > MaxJsonDepth)
    return ""; // Safety limit exceeded

  std::string IndentStr(Indent * 2, ' ');
  std::string NextIndent((Indent + 1) * 2, ' ');
  std::string Out;

  if (V.kind() == llvm::json::Value::Null) {
    Out = "null";
  } else if (auto B = V.getAsBoolean()) {
    Out = *B ? "true" : "false";
  } else if (auto I = V.getAsInteger()) {
    Out = std::to_string(*I);
  } else if (auto D = V.getAsNumber()) {
    // Format floating point with enough precision
    std::ostringstream SS;
    SS << std::setprecision(17) << *D;
    Out = SS.str();
  } else if (auto S = V.getAsString()) {
    Out = "\"" + escapeNixString(*S) + "\"";
  } else if (const auto *A = V.getAsArray()) {
    if (A->size() > MaxJsonWidth)
      return ""; // Width limit exceeded
    if (A->empty()) {
      Out = "[ ]";
    } else {
      // Pre-allocate memory to reduce reallocations
      // Estimate: opening + closing + elements * (indent + value_estimate +
      // newline)
      size_t EstimatedSize = 4 + A->size() * ((Indent + 1) * 2 + 20);
      Out.reserve(EstimatedSize);
      Out = "[\n";
      for (size_t I = 0; I < A->size(); ++I) {
        std::string Elem = jsonToNix((*A)[I], Indent + 1, Depth + 1);
        if (Elem.empty())
          return ""; // Propagate error
        Out += NextIndent + Elem;
        if (I + 1 < A->size())
          Out += "\n";
      }
      Out += "\n" + IndentStr + "]";
    }
  } else if (const auto *O = V.getAsObject()) {
    if (O->size() > MaxJsonWidth)
      return ""; // Width limit exceeded
    if (O->empty()) {
      Out = "{ }";
    } else {
      // Pre-allocate memory to reduce reallocations
      // Estimate: braces + elements * (indent + key + " = " + value_estimate +
      // ";\n")
      size_t EstimatedSize = 4 + O->size() * ((Indent + 1) * 2 + 30);
      Out.reserve(EstimatedSize);
      Out = "{\n";
      size_t I = 0;
      for (const auto &KV : *O) {
        std::string Key = quoteNixAttrKey(KV.first.str());
        std::string Val = jsonToNix(KV.second, Indent + 1, Depth + 1);
        if (Val.empty())
          return ""; // Propagate error
        Out += NextIndent + Key + " = " + Val + ";";
        if (I + 1 < O->size())
          Out += "\n";
        ++I;
      }
      Out += "\n" + IndentStr + "}";
    }
  }
  return Out;
}

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

/// \brief Add flatten action for nested attribute sets.
/// Transforms: { foo = { bar = 1; }; } -> { foo.bar = 1; }
void addFlattenAttrsAction(const nixf::Node &N,
                           const nixf::ParentMapAnalysis &PM,
                           const std::string &FileURI, llvm::StringRef Src,
                           std::vector<CodeAction> &Actions) {
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
      "Flatten nested attribute set", CodeAction::REFACTOR_REWRITE_KIND,
      FileURI, toLSPRange(Src, Bind.range()), std::move(NewText)));
}

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

/// \brief Maximum recursion depth for nested text generation.
/// Prevents stack overflow on maliciously crafted deeply nested inputs.
constexpr size_t MaxNestedDepth = 100;

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

/// \brief Add pack action for dotted attribute paths.
/// Transforms: { foo.bar = 1; } -> { foo = { bar = 1; }; }
/// Also offers bulk pack when siblings share a prefix:
/// { foo.bar = 1; foo.baz = 2; } -> { foo = { bar = 1; baz = 2; }; }
void addPackAttrsAction(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        const std::string &FileURI, llvm::StringRef Src,
                        std::vector<CodeAction> &Actions) {
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
        "Pack dotted path to nested set", CodeAction::REFACTOR_REWRITE_KIND,
        FileURI, toLSPRange(Src, Bind.range()), std::move(NewText)));
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
          "Pack dotted path to nested set", CodeAction::REFACTOR_REWRITE_KIND,
          FileURI, toLSPRange(Src, Bind.range()), std::move(PackOneText)));
    }

    // Action 2: Shallow Pack All - pack all siblings but only one level deep
    std::string ShallowText;
    ShallowText += quoteNixAttrKey(FirstSeg);
    ShallowText += " = ";
    generateShallowNestedText(ParentBinds, FirstSeg, Src, ShallowText);
    ShallowText += ";";

    Actions.emplace_back(createSingleEditAction(
        "Pack all '" + FirstSeg + "' bindings to nested set",
        CodeAction::REFACTOR_REWRITE_KIND, FileURI, toLSPRange(Src, *BulkRange),
        std::move(ShallowText)));

    // Action 3: Recursive Pack All - fully nest all sibling bindings
    std::string RecursiveText;
    RecursiveText += quoteNixAttrKey(FirstSeg);
    RecursiveText += " = ";
    generateNestedText(NestedAttrs.sema(), Src, RecursiveText);
    RecursiveText += ";";

    Actions.emplace_back(createSingleEditAction(
        "Recursively pack all '" + FirstSeg + "' bindings to nested set",
        CodeAction::REFACTOR_REWRITE_KIND, FileURI, toLSPRange(Src, *BulkRange),
        std::move(RecursiveText)));
  }
}

/// \brief Add refactoring code actions for attribute names (quote/unquote).
void addAttrNameActions(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        const std::string &FileURI, llvm::StringRef Src,
                        std::vector<CodeAction> &Actions) {
  // Find if we're inside an AttrName
  const nixf::Node *AttrNameNode = PM.upTo(N, nixf::Node::NK_AttrName);
  if (!AttrNameNode)
    return;

  const auto &AN = static_cast<const nixf::AttrName &>(*AttrNameNode);

  if (AN.kind() == nixf::AttrName::ANK_ID) {
    // Offer to quote: foo -> "foo"
    const std::string &Name = AN.id()->name();
    Actions.emplace_back(createSingleEditAction(
        "Quote attribute name", CodeAction::REFACTOR_REWRITE_KIND, FileURI,
        toLSPRange(Src, AN.range()), "\"" + Name + "\""));
  } else if (AN.kind() == nixf::AttrName::ANK_String && AN.isStatic()) {
    // Offer to unquote if valid identifier: "foo" -> foo
    const std::string &Name = AN.staticName();
    if (isValidNixIdentifier(Name)) {
      Actions.emplace_back(createSingleEditAction(
          "Unquote attribute name", CodeAction::REFACTOR_REWRITE_KIND, FileURI,
          toLSPRange(Src, AN.range()), Name));
    }
  }
}

/// \brief Add JSON to Nix conversion action for selected JSON text.
/// This is a selection-based action that works on arbitrary text, not AST
/// nodes.
void addJsonToNixAction(llvm::StringRef Src, const lspserver::Range &Range,
                        const std::string &FileURI,
                        std::vector<CodeAction> &Actions) {
  // Convert LSP positions to byte offsets
  llvm::Expected<size_t> StartOffset = positionToOffset(Src, Range.start);
  llvm::Expected<size_t> EndOffset = positionToOffset(Src, Range.end);

  if (!StartOffset || !EndOffset) {
    if (!StartOffset)
      llvm::consumeError(StartOffset.takeError());
    if (!EndOffset)
      llvm::consumeError(EndOffset.takeError());
    return;
  }

  // Validate range
  if (*StartOffset >= *EndOffset || *EndOffset > Src.size())
    return;

  // Extract selected text
  llvm::StringRef SelectedText =
      Src.substr(*StartOffset, *EndOffset - *StartOffset);

  // Skip if selection is too short (minimum valid JSON is "{}" or "[]")
  if (SelectedText.size() < 2)
    return;

  // Skip if first character is not { or [ (quick rejection)
  char First = SelectedText.front();
  if (First != '{' && First != '[')
    return;

  // Try to parse as JSON
  llvm::Expected<llvm::json::Value> JsonVal = llvm::json::parse(SelectedText);
  if (!JsonVal) {
    llvm::consumeError(JsonVal.takeError());
    return;
  }

  // Skip empty JSON structures - already valid Nix
  if (const auto *A = JsonVal->getAsArray()) {
    if (A->empty())
      return;
  } else if (const auto *O = JsonVal->getAsObject()) {
    if (O->empty())
      return;
  }

  // Convert JSON to Nix
  std::string NixText = jsonToNix(*JsonVal);
  if (NixText.empty())
    return;

  Actions.emplace_back(createSingleEditAction(
      "Convert JSON to Nix", CodeAction::REFACTOR_REWRITE_KIND, FileURI, Range,
      std::move(NixText)));
}

} // namespace

void Controller::onCodeAction(const lspserver::CodeActionParams &Params,
                              Callback<std::vector<CodeAction>> Reply) {
  using CheckTy = std::vector<CodeAction>;
  std::string File(Params.textDocument.uri.file());
  Range Range = Params.range;
  auto Action = [Reply = std::move(Reply), File, Range, this]() mutable {
    return Reply([&]() -> llvm::Expected<CheckTy> {
      const auto TU = CheckDefault(getTU(File));

      const auto &Diagnostics = TU->diagnostics();
      auto Actions = std::vector<CodeAction>();
      Actions.reserve(Diagnostics.size());
      std::string FileURI = URIForFile::canonicalize(File, File).uri();

      for (const nixf::Diagnostic &D : Diagnostics) {
        auto DRange = toLSPRange(TU->src(), D.range());
        if (!Range.overlap(DRange))
          continue;

        // Add fixes.
        for (const nixf::Fix &F : D.fixes()) {
          std::vector<TextEdit> Edits;
          Edits.reserve(F.edits().size());
          for (const nixf::TextEdit &TE : F.edits()) {
            Edits.emplace_back(TextEdit{
                .range = toLSPRange(TU->src(), TE.oldRange()),
                .newText = std::string(TE.newText()),
            });
          }
          using Changes = std::map<std::string, std::vector<TextEdit>>;
          WorkspaceEdit WE{.changes = Changes{
                               {FileURI, std::move(Edits)},
                           }};
          Actions.emplace_back(CodeAction{
              .title = F.message(),
              .kind = std::string(CodeAction::QUICKFIX_KIND),
              .edit = std::move(WE),
          });
        }
      }

      // Add refactoring code actions based on cursor position
      if (TU->ast() && TU->parentMap()) {
        nixf::PositionRange NixfRange = toNixfRange(Range);
        if (const nixf::Node *N = TU->ast()->descend(NixfRange)) {
          addAttrNameActions(*N, *TU->parentMap(), FileURI, TU->src(), Actions);
          addFlattenAttrsAction(*N, *TU->parentMap(), FileURI, TU->src(),
                                Actions);
          addPackAttrsAction(*N, *TU->parentMap(), FileURI, TU->src(), Actions);
        }
      }

      // Selection-based actions (work on arbitrary text, not AST nodes)
      addJsonToNixAction(TU->src(), Range, FileURI, Actions);

      return Actions;
    }());
  };
  boost::asio::post(Pool, std::move(Action));
}

} // namespace nixd
