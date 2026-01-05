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

#include <set>

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

/// \brief Check if sibling bindings share the same first path segment.
/// Used to block Pack action when conflicts would occur.
bool hasConflictingSiblings(const nixf::Binding &Bind,
                            const nixf::ParentMapAnalysis &PM) {
  // Get the first segment of this binding's path
  const auto &Names = Bind.path().names();
  if (Names.empty() || !Names[0]->isStatic())
    return true; // Can't determine, block action

  const std::string &FirstSeg = Names[0]->staticName();

  // Find parent Binds node
  const nixf::Node *Parent = PM.query(Bind);
  if (!Parent || Parent->kind() != nixf::Node::NK_Binds)
    return true; // Can't find parent, block action

  const auto &ParentBinds = static_cast<const nixf::Binds &>(*Parent);

  // Check all sibling bindings for conflicts
  for (const auto &Sibling : ParentBinds.bindings()) {
    if (Sibling.get() == &Bind)
      continue; // Skip self

    if (Sibling->kind() != nixf::Node::NK_Binding)
      continue; // Skip Inherit nodes

    const auto &SibBind = static_cast<const nixf::Binding &>(*Sibling);
    const auto &SibNames = SibBind.path().names();

    if (SibNames.empty())
      continue;

    // Check if first segment matches
    if (SibNames[0]->isStatic() && SibNames[0]->staticName() == FirstSeg)
      return true; // Conflict found
  }

  return false;
}

/// \brief Add pack action for dotted attribute paths.
/// Transforms: { foo.bar = 1; } -> { foo = { bar = 1; }; }
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

  // Check for sibling conflicts
  if (hasConflictingSiblings(Bind, PM))
    return;

  // Build the packed text: first = { rest = value; }
  std::string NewText;

  // First segment
  const std::string_view FirstName = Names[0]->src(Src);

  // Pre-allocate to reduce reallocations
  size_t ValueSize = Bind.value() ? Bind.value()->src(Src).size() : 0;
  NewText.reserve(FirstName.size() + Bind.path().src(Src).size() + ValueSize +
                  15); // 15 for " = { ", " = ", "; };"
  NewText += FirstName;
  NewText += " = { ";

  // Remaining path segments (from second name to end)
  // Get source from second name start to path end
  const nixf::LexerCursor RestStart = Names[1]->range().lCur();
  const nixf::LexerCursor RestEnd = Bind.path().range().rCur();
  std::string_view RestPath =
      Src.substr(RestStart.offset(), RestEnd.offset() - RestStart.offset());

  NewText += RestPath;
  NewText += " = ";

  // Value
  if (Bind.value()) {
    NewText += Bind.value()->src(Src);
  }
  NewText += "; };";

  Actions.emplace_back(createSingleEditAction(
      "Pack dotted path to nested set", CodeAction::REFACTOR_REWRITE_KIND,
      FileURI, toLSPRange(Src, Bind.range()), std::move(NewText)));
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

      return Actions;
    }());
  };
  boost::asio::post(Pool, std::move(Action));
}

} // namespace nixd
