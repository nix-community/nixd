/// \file
/// \brief Implementation of [Code Action].
/// [Code Action]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_codeAction

#include "AST.h"
#include "CheckReturn.h"
#include "Convert.h"

#include "nixd/Controller/Controller.h"

#include <nixf/Basic/Nodes/Attrs.h>
#include <nixf/Basic/Nodes/Expr.h>
#include <nixf/Basic/Nodes/Simple.h>
#include <nixf/Sema/ParentMap.h>
#include <nixf/Sema/VariableLookup.h>

#include <boost/asio/post.hpp>
#include <lspserver/SourceCode.h>
#include <llvm/Support/JSON.h>

#include <set>
#include <sstream>

namespace nixd {

using namespace llvm::json;
using namespace lspserver;

namespace {

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

  // Check against Nix keywords
  static const std::set<std::string> Keywords = {
      "if", "then", "else", "let", "in", "with", "rec", "inherit", "assert",
      "or"};
  return Keywords.find(S) == Keywords.end();
}

/// \brief Add refactoring code actions for attribute names (quote/unquote).
void addAttrNameActions(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        const URIForFile &URI, llvm::StringRef Src,
                        std::vector<CodeAction> &Actions) {
  // Find if we're inside an AttrName
  const nixf::Node *AttrNameNode = PM.upTo(N, nixf::Node::NK_AttrName);
  if (!AttrNameNode)
    return;

  // Only offer quote/unquote for attribute sets, not for let bindings.
  // ExprLet internally contains an ExprAttrs for bindings, so we need to
  // check if the parent ExprAttrs is directly inside an ExprLet.
  const nixf::Node *ExprAttrsNode = PM.upTo(*AttrNameNode, nixf::Node::NK_ExprAttrs);
  if (!ExprAttrsNode)
    return;

  // Check if this ExprAttrs is the binds part of an ExprLet
  const nixf::Node *AttrsParent = PM.query(*ExprAttrsNode);
  if (AttrsParent && AttrsParent->kind() == nixf::Node::NK_ExprLet)
    return;

  const auto &AN = static_cast<const nixf::AttrName &>(*AttrNameNode);

  std::string FileURI = URI.uri();

  if (AN.kind() == nixf::AttrName::ANK_ID) {
    // Offer to quote: foo -> "foo"
    const std::string &Name = AN.id()->name();
    std::string NewText = "\"" + Name + "\"";

    std::vector<lspserver::TextEdit> Edits;
    Edits.emplace_back(lspserver::TextEdit{
        .range = toLSPRange(Src, AN.range()),
        .newText = NewText,
    });

    using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
    WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};

    Actions.emplace_back(CodeAction{
        .title = "Quote attribute name",
        .kind = std::string(CodeAction::REFACTOR_KIND),
        .edit = std::move(WE),
    });
  } else if (AN.kind() == nixf::AttrName::ANK_String && AN.isStatic()) {
    // Offer to unquote if valid identifier: "foo" -> foo
    const std::string &Name = AN.staticName();
    if (isValidNixIdentifier(Name)) {
      std::vector<lspserver::TextEdit> Edits;
      Edits.emplace_back(lspserver::TextEdit{
          .range = toLSPRange(Src, AN.range()),
          .newText = Name,
      });

      using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
      WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};

      Actions.emplace_back(CodeAction{
          .title = "Unquote attribute name",
          .kind = std::string(CodeAction::REFACTOR_KIND),
          .edit = std::move(WE),
      });
    }
  }
}

/// \brief Extract source text from a node's range.
std::string getSourceText(llvm::StringRef Src, const nixf::Node &N) {
  size_t Start = N.range().lCur().offset();
  size_t End = N.range().rCur().offset();

  // Validate bounds to prevent undefined behavior
  if (Start >= Src.size() || End > Src.size() || Start >= End)
    return "";

  return std::string(Src.substr(Start, End - Start));
}

/// \brief Check if a binding can be flattened (value is a simple attrset).
/// Returns the nested ExprAttrs if flattenable, nullptr otherwise.
const nixf::ExprAttrs *getFlattenableNestedAttrs(const nixf::Binding &B) {
  if (!B.value())
    return nullptr;

  // Value must be an ExprAttrs
  const nixf::Expr *Value = B.value().get();
  if (Value->kind() != nixf::Node::NK_ExprAttrs)
    return nullptr;

  const auto *NestedAttrs = static_cast<const nixf::ExprAttrs *>(Value);

  // Don't flatten recursive attribute sets
  if (NestedAttrs->isRecursive())
    return nullptr;

  // Must have bindings
  if (!NestedAttrs->binds())
    return nullptr;

  const auto &Bindings = NestedAttrs->binds()->bindings();
  if (Bindings.empty())
    return nullptr;

  // All bindings must be regular Binding nodes (not Inherit)
  for (const auto &BindNode : Bindings) {
    if (BindNode->kind() != nixf::Node::NK_Binding)
      return nullptr;

    const auto &Bind = static_cast<const nixf::Binding &>(*BindNode);
    // Path must be static (no dynamic attributes)
    for (const auto &Name : Bind.path().names()) {
      if (!Name->isStatic())
        return nullptr;
    }
  }

  return NestedAttrs;
}

/// \brief Add flatten code action for nested attribute sets.
/// Example: { foo = { bar = 1; }; } -> { foo.bar = 1; }
void addFlattenAction(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                      const URIForFile &URI, llvm::StringRef Src,
                      std::vector<CodeAction> &Actions) {
  // Find if we're inside a Binding
  const nixf::Node *BindingNode = PM.upTo(N, nixf::Node::NK_Binding);
  if (!BindingNode)
    return;

  const auto &Bind = static_cast<const nixf::Binding &>(*BindingNode);

  // Outer path must be static (no dynamic attributes)
  for (const auto &Name : Bind.path().names()) {
    if (!Name->isStatic())
      return;
  }

  // Check if binding value is a flattenable nested attrset
  const nixf::ExprAttrs *NestedAttrs = getFlattenableNestedAttrs(Bind);
  if (!NestedAttrs)
    return;

  // Build the flattened bindings
  std::ostringstream NewText;
  const auto &OuterNames = Bind.path().names();
  const auto &NestedBindings = NestedAttrs->binds()->bindings();

  for (size_t I = 0; I < NestedBindings.size(); ++I) {
    const auto &NestedBind =
        static_cast<const nixf::Binding &>(*NestedBindings[I]);

    // Build the combined path: outer.inner
    for (const auto &Name : OuterNames) {
      NewText << getSourceText(Src, *Name);
      NewText << ".";
    }

    // Add the nested path and value
    NewText << getSourceText(Src, NestedBind.path());
    NewText << " = ";
    if (NestedBind.value()) {
      NewText << getSourceText(Src, *NestedBind.value());
    }
    NewText << ";";

    if (I + 1 < NestedBindings.size())
      NewText << " ";
  }

  std::string FileURI = URI.uri();
  std::vector<lspserver::TextEdit> Edits;
  Edits.emplace_back(lspserver::TextEdit{
      .range = toLSPRange(Src, Bind.range()),
      .newText = NewText.str(),
  });

  using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
  WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};

  Actions.emplace_back(CodeAction{
      .title = "Flatten attribute set",
      .kind = std::string(CodeAction::REFACTOR_KIND),
      .edit = std::move(WE),
  });
}

/// \brief Add pack code action for dotted attribute paths.
/// Example: { foo.bar = 1; } -> { foo = { bar = 1; }; }
void addPackAction(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                   const URIForFile &URI, llvm::StringRef Src,
                   std::vector<CodeAction> &Actions) {
  // Find if we're inside a Binding
  const nixf::Node *BindingNode = PM.upTo(N, nixf::Node::NK_Binding);
  if (!BindingNode)
    return;

  const auto &Bind = static_cast<const nixf::Binding &>(*BindingNode);

  // Path must have more than one name to be packable
  const auto &Names = Bind.path().names();
  if (Names.size() < 2)
    return;

  // All names must be static
  for (const auto &Name : Names) {
    if (!Name->isStatic())
      return;
  }

  // Build the packed binding: first_name = { rest.of.path = value; };
  std::ostringstream NewText;

  // First name
  NewText << getSourceText(Src, *Names[0]);
  NewText << " = { ";

  // Rest of path
  for (size_t I = 1; I < Names.size(); ++I) {
    NewText << getSourceText(Src, *Names[I]);
    if (I + 1 < Names.size())
      NewText << ".";
  }

  // Value
  NewText << " = ";
  if (Bind.value()) {
    NewText << getSourceText(Src, *Bind.value());
  }
  NewText << "; };";

  std::string FileURI = URI.uri();
  std::vector<lspserver::TextEdit> Edits;
  Edits.emplace_back(lspserver::TextEdit{
      .range = toLSPRange(Src, Bind.range()),
      .newText = NewText.str(),
  });

  using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
  WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};

  Actions.emplace_back(CodeAction{
      .title = "Pack attribute set",
      .kind = std::string(CodeAction::REFACTOR_KIND),
      .edit = std::move(WE),
  });
}

/// \brief Convert a JSON value to Nix expression syntax.
std::string jsonToNix(const llvm::json::Value &V, int Indent = 0) {
  std::ostringstream Out;
  std::string IndentStr(Indent * 2, ' ');
  std::string NextIndent((Indent + 1) * 2, ' ');

  if (V.kind() == llvm::json::Value::Null) {
    Out << "null";
  } else if (auto B = V.getAsBoolean()) {
    Out << (*B ? "true" : "false");
  } else if (auto I = V.getAsInteger()) {
    Out << *I;
  } else if (auto D = V.getAsNumber()) {
    Out << *D;
  } else if (auto S = V.getAsString()) {
    // Escape the string for Nix
    Out << "\"";
    for (char C : *S) {
      switch (C) {
      case '\\':
        Out << "\\\\";
        break;
      case '"':
        Out << "\\\"";
        break;
      case '\n':
        Out << "\\n";
        break;
      case '\r':
        Out << "\\r";
        break;
      case '\t':
        Out << "\\t";
        break;
      case '$':
        Out << "\\$";
        break;
      default:
        Out << C;
      }
    }
    Out << "\"";
  } else if (const auto *A = V.getAsArray()) {
    if (A->empty()) {
      Out << "[ ]";
    } else {
      Out << "[\n";
      for (size_t I = 0; I < A->size(); ++I) {
        Out << NextIndent << jsonToNix((*A)[I], Indent + 1);
        if (I + 1 < A->size())
          Out << "\n";
      }
      Out << "\n" << IndentStr << "]";
    }
  } else if (const auto *O = V.getAsObject()) {
    if (O->empty()) {
      Out << "{ }";
    } else {
      Out << "{\n";
      size_t I = 0;
      for (const auto &KV : *O) {
        std::string Key = KV.first.str();
        // Quote the key if it's not a valid identifier
        if (isValidNixIdentifier(Key)) {
          Out << NextIndent << Key;
        } else {
          Out << NextIndent << "\"" << Key << "\"";
        }
        Out << " = " << jsonToNix(KV.second, Indent + 1) << ";";
        if (I + 1 < O->size())
          Out << "\n";
        ++I;
      }
      Out << "\n" << IndentStr << "}";
    }
  }
  return Out.str();
}

/// \brief Add JSON to Nix conversion code action for selected JSON text.
void addJsonToNixAction(llvm::StringRef Src, lspserver::Range SelectionRange,
                        const URIForFile &URI,
                        std::vector<CodeAction> &Actions) {
  // Use lspserver's positionToOffset for proper UTF-16 handling
  llvm::Expected<size_t> StartOffsetExp =
      positionToOffset(Src, SelectionRange.start);
  llvm::Expected<size_t> EndOffsetExp =
      positionToOffset(Src, SelectionRange.end);

  if (!StartOffsetExp || !EndOffsetExp) {
    if (!StartOffsetExp)
      llvm::consumeError(StartOffsetExp.takeError());
    if (!EndOffsetExp)
      llvm::consumeError(EndOffsetExp.takeError());
    return;
  }

  size_t StartOffset = *StartOffsetExp;
  size_t EndOffset = *EndOffsetExp;

  if (StartOffset >= EndOffset)
    return;

  std::string SelectedText =
      std::string(Src.substr(StartOffset, EndOffset - StartOffset));

  // Skip if selection is empty or too short
  if (SelectedText.empty() || SelectedText.size() < 2)
    return;

  // Try to parse as JSON
  llvm::Expected<llvm::json::Value> Parsed = llvm::json::parse(SelectedText);
  if (!Parsed) {
    llvm::consumeError(Parsed.takeError());
    return;
  }

  // Convert to Nix
  std::string NixText = jsonToNix(*Parsed);

  std::string FileURI = URI.uri();
  std::vector<lspserver::TextEdit> Edits;
  Edits.emplace_back(lspserver::TextEdit{
      .range = SelectionRange,
      .newText = NixText,
  });

  using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
  WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};

  Actions.emplace_back(CodeAction{
      .title = "Convert JSON to Nix",
      .kind = std::string(CodeAction::REFACTOR_KIND),
      .edit = std::move(WE),
  });
}

/// \brief Add "Open in noogle.dev" action for lib function references.
/// Example: lib.mapAttrs -> opens https://noogle.dev/f/lib/mapAttrs
void addNoogleAction(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                     const nixf::VariableLookupAnalysis &VLA,
                     std::vector<CodeAction> &Actions) {
  // Find if we're inside an ExprSelect (e.g., lib.mapAttrs)
  const nixf::Node *SelectNode = PM.upTo(N, nixf::Node::NK_ExprSelect);
  if (!SelectNode)
    return;

  const auto &Select = static_cast<const nixf::ExprSelect &>(*SelectNode);

  // Try to build a selector using the idiom system
  try {
    Selector Sel = idioms::mkSelector(Select, VLA, PM);

    // Check if it starts with "lib" (nixpkgs lib functions)
    // Need at least "lib" + function name (e.g., "lib.mapAttrs")
    if (Sel.size() < 2 || Sel[0] != idioms::Lib)
      return;

    // Build the noogle.dev URL
    // Format: https://noogle.dev/f/lib/<path>
    std::ostringstream Url;
    Url << "https://noogle.dev/f/lib";
    for (size_t I = 1; I < Sel.size(); ++I) {
      Url << "/" << Sel[I];
    }

    // Build the function name for display
    std::ostringstream FuncName;
    FuncName << "lib";
    for (size_t I = 1; I < Sel.size(); ++I) {
      FuncName << "." << Sel[I];
    }

    // Create a command-based code action
    // The editor/client is responsible for executing this command
    Command Cmd;
    Cmd.command = "nixd.openNoogle";
    Cmd.title = "Open " + FuncName.str() + " in noogle.dev";
    Cmd.argument = Url.str();

    Actions.emplace_back(CodeAction{
        .title = "Open " + FuncName.str() + " in noogle.dev",
        .kind = std::string(CodeAction::REFACTOR_KIND),
        .command = std::move(Cmd),
    });
  } catch (const idioms::IdiomSelectorException &) {
    // Not a recognized idiom, skip
  }
}

/// \brief Add "Extract expression to file" action for selected expressions.
/// Creates a command that the client should handle to:
/// 1. Prompt the user for a file path
/// 2. Create the new file with the expression
/// 3. Replace the selection with an import statement
void addExtractToFileAction(const nixf::Node &N, const URIForFile &URI,
                            llvm::StringRef Src,
                            std::vector<CodeAction> &Actions) {
  // Only offer extract for substantial expression types
  // Skip for simple nodes like identifiers, keywords, literals
  switch (N.kind()) {
  case nixf::Node::NK_ExprAttrs:
  case nixf::Node::NK_ExprList:
  case nixf::Node::NK_ExprLambda:
  case nixf::Node::NK_ExprCall:
  case nixf::Node::NK_ExprLet:
  case nixf::Node::NK_ExprIf:
  case nixf::Node::NK_ExprWith:
  case nixf::Node::NK_ExprSelect:
    break;
  default:
    return;
  }

  // Get the expression's source text
  std::string ExprText = getSourceText(Src, N);

  // Skip if expression is too short
  if (ExprText.size() < 5)
    return;

  // Create a command for the client to handle
  // The command includes the expression text and source file URI
  Command Cmd;
  Cmd.command = "nixd.extractToFile";
  Cmd.title = "Extract expression to file";

  // Build a JSON object with the necessary info
  // Use toLSPRange for proper UTF-16 offset conversion
  lspserver::Range LspRange = toLSPRange(Src, N.range());
  llvm::json::Object Args;
  Args["expression"] = ExprText;
  Args["uri"] = URI.uri();
  Args["range"] = llvm::json::Object{
      {"start", llvm::json::Object{{"line", LspRange.start.line},
                                   {"character", LspRange.start.character}}},
      {"end", llvm::json::Object{{"line", LspRange.end.line},
                                 {"character", LspRange.end.character}}}};
  Cmd.argument = llvm::json::Value(std::move(Args));

  Actions.emplace_back(CodeAction{
      .title = "Extract expression to file",
      .kind = std::string(CodeAction::REFACTOR_KIND),
      .command = std::move(Cmd),
  });
}

/// \brief Check if an expression contains a nested `with` expression.
bool hasNestedWith(const nixf::Node &N) {
  if (N.kind() == nixf::Node::NK_ExprWith)
    return true;

  for (const nixf::Node *Child : N.children()) {
    if (Child && hasNestedWith(*Child))
      return true;
  }
  return false;
}

/// \brief Collect all variable names that come from a `with` expression.
void collectWithVariables(const nixf::Node &N,
                          const nixf::VariableLookupAnalysis &VLA,
                          std::set<std::string> &Names) {
  // If this is a variable, check if it comes from `with`
  if (N.kind() == nixf::Node::NK_ExprVar) {
    const auto &Var = static_cast<const nixf::ExprVar &>(N);
    auto Result = VLA.query(Var);
    if (Result.Kind == nixf::VariableLookupAnalysis::LookupResultKind::FromWith) {
      Names.insert(Var.id().name());
    }
  }

  // Recursively process children
  for (const nixf::Node *Child : N.children()) {
    if (Child)
      collectWithVariables(*Child, VLA, Names);
  }
}

/// \brief Add "Convert with to let/inherit" action for with expressions.
/// Example: with lib; { x = foo; } -> let inherit (lib) foo; in { x = foo; }
void addWithToLetAction(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        const nixf::VariableLookupAnalysis &VLA,
                        const URIForFile &URI, llvm::StringRef Src,
                        std::vector<CodeAction> &Actions) {
  // Find if we're inside an ExprWith
  const nixf::Node *WithNode = PM.upTo(N, nixf::Node::NK_ExprWith);
  if (!WithNode)
    return;

  const auto &With = static_cast<const nixf::ExprWith &>(*WithNode);

  // Need both the with expression and body
  if (!With.with() || !With.expr())
    return;

  // Don't offer conversion for nested `with` expressions
  // because we can't reliably determine which `with` each variable comes from
  if (hasNestedWith(*With.expr()))
    return;

  // Collect all variables that come from this `with`
  std::set<std::string> VarNames;
  collectWithVariables(*With.expr(), VLA, VarNames);

  // Skip if no variables from with
  if (VarNames.empty())
    return;

  // Build the replacement text
  std::ostringstream NewText;
  NewText << "let inherit (" << getSourceText(Src, *With.with()) << ")";
  for (const auto &Name : VarNames) {
    NewText << " " << Name;
  }
  NewText << "; in " << getSourceText(Src, *With.expr());

  std::string FileURI = URI.uri();
  std::vector<lspserver::TextEdit> Edits;
  Edits.emplace_back(lspserver::TextEdit{
      .range = toLSPRange(Src, With.range()),
      .newText = NewText.str(),
  });

  using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
  WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};

  Actions.emplace_back(CodeAction{
      .title = "Convert with to let/inherit",
      .kind = std::string(CodeAction::REFACTOR_KIND),
      .edit = std::move(WE),
  });
}

} // namespace

void Controller::onCodeAction(const lspserver::CodeActionParams &Params,
                              Callback<std::vector<CodeAction>> Reply) {
  using CheckTy = std::vector<CodeAction>;
  std::string File(Params.textDocument.uri.file());
  Range Range = Params.range;
  URIForFile URI = Params.textDocument.uri;
  auto Action = [Reply = std::move(Reply), File, Range, URI, this]() mutable {
    return Reply([&]() -> llvm::Expected<CheckTy> {
      const auto TU = CheckDefault(getTU(File));

      const auto &Diagnostics = TU->diagnostics();
      auto Actions = std::vector<CodeAction>();
      Actions.reserve(Diagnostics.size());

      // Add diagnostic-based quick fixes
      for (const nixf::Diagnostic &D : Diagnostics) {
        auto DRange = toLSPRange(TU->src(), D.range());
        if (!Range.overlap(DRange))
          continue;

        // Add fixes.
        for (const nixf::Fix &F : D.fixes()) {
          std::vector<lspserver::TextEdit> Edits;
          Edits.reserve(F.edits().size());
          for (const nixf::TextEdit &TE : F.edits()) {
            Edits.emplace_back(lspserver::TextEdit{
                .range = toLSPRange(TU->src(), TE.oldRange()),
                .newText = std::string(TE.newText()),
            });
          }
          using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
          std::string FileURI = URIForFile::canonicalize(File, File).uri();
          WorkspaceEdit WE{.changes = Changes{
                               {std::move(FileURI), std::move(Edits)},
                           }};
          Actions.emplace_back(CodeAction{
              .title = F.message(),
              .kind = std::string(CodeAction::QUICKFIX_KIND),
              .edit = std::move(WE),
          });
        }
      }

      // Add refactoring actions
      if (TU->ast() && TU->parentMap()) {
        nixf::Position Pos = toNixfPosition(Range.start);
        if (const nixf::Node *N = TU->ast()->descend({Pos, Pos})) {
          addAttrNameActions(*N, *TU->parentMap(), URI, TU->src(), Actions);
          addFlattenAction(*N, *TU->parentMap(), URI, TU->src(), Actions);
          addPackAction(*N, *TU->parentMap(), URI, TU->src(), Actions);
          if (TU->variableLookup()) {
            addNoogleAction(*N, *TU->parentMap(), *TU->variableLookup(),
                            Actions);
            addWithToLetAction(*N, *TU->parentMap(), *TU->variableLookup(), URI,
                               TU->src(), Actions);
          }
          addExtractToFileAction(*N, URI, TU->src(), Actions);
        }
      }

      // Add JSON to Nix conversion action (selection-based)
      addJsonToNixAction(TU->src(), Range, URI, Actions);

      return Actions;
    }());
  };
  boost::asio::post(Pool, std::move(Action));
}

} // namespace nixd
