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
#include <llvm/Support/JSON.h>
#include <lspserver/SourceCode.h>

#include <iomanip>
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
      "if",   "then", "else",    "let",    "in",
      "with", "rec",  "inherit", "assert", "or"};
  return Keywords.find(S) == Keywords.end();
}

/// \brief Percent-encode a string for use in URL path segments.
/// Encodes all characters except unreserved characters (RFC 3986):
/// ALPHA / DIGIT / "-" / "." / "_" / "~"
std::string percentEncode(const std::string &S) {
  std::ostringstream Encoded;
  Encoded << std::hex << std::uppercase;

  for (unsigned char C : S) {
    // Unreserved characters (RFC 3986 section 2.3)
    if ((C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') ||
        (C >= '0' && C <= '9') || C == '-' || C == '.' || C == '_' ||
        C == '~') {
      Encoded << C;
    } else {
      // Percent-encode everything else
      Encoded << '%' << std::setw(2) << std::setfill('0')
              << static_cast<int>(C);
    }
  }

  return Encoded.str();
}

/// \brief Create a WorkspaceEdit with a single file's text edits.
/// This helper reduces boilerplate when creating code actions.
WorkspaceEdit createWorkspaceEdit(const std::string &FileURI,
                                  std::vector<lspserver::TextEdit> Edits) {
  using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
  return WorkspaceEdit{.changes = Changes{{FileURI, std::move(Edits)}}};
}

/// \brief Create a single TextEdit for a range replacement.
lspserver::TextEdit createTextEdit(llvm::StringRef Src,
                                   const nixf::LexerCursorRange &Range,
                                   std::string NewText) {
  return lspserver::TextEdit{
      .range = toLSPRange(Src, Range),
      .newText = std::move(NewText),
  };
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
  const nixf::Node *ExprAttrsNode =
      PM.upTo(*AttrNameNode, nixf::Node::NK_ExprAttrs);
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
    Actions.emplace_back(CodeAction{
        .title = "Quote attribute name",
        .kind = std::string(CodeAction::REFACTOR_REWRITE_KIND),
        .edit = createWorkspaceEdit(
            FileURI, {createTextEdit(Src, AN.range(), "\"" + Name + "\"")}),
    });
  } else if (AN.kind() == nixf::AttrName::ANK_String && AN.isStatic()) {
    // Offer to unquote if valid identifier: "foo" -> foo
    const std::string &Name = AN.staticName();
    if (isValidNixIdentifier(Name)) {
      Actions.emplace_back(CodeAction{
          .title = "Unquote attribute name",
          .kind = std::string(CodeAction::REFACTOR_REWRITE_KIND),
          .edit = createWorkspaceEdit(FileURI,
                                      {createTextEdit(Src, AN.range(), Name)}),
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
      .kind = std::string(CodeAction::REFACTOR_REWRITE_KIND),
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
      .kind = std::string(CodeAction::REFACTOR_REWRITE_KIND),
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
      .kind = std::string(CodeAction::REFACTOR_REWRITE_KIND),
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

    // Build the noogle.dev URL with percent-encoding
    // Format: https://noogle.dev/f/lib/<path>
    std::ostringstream Url;
    Url << "https://noogle.dev/f/lib";
    for (size_t I = 1; I < Sel.size(); ++I) {
      Url << "/" << percentEncode(Sel[I]);
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
        .kind = std::string(CodeAction::INFO_KIND),
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
      .kind = std::string(CodeAction::REFACTOR_EXTRACT_KIND),
      .command = std::move(Cmd),
  });
}

/// \brief Escape a string for double-quoted Nix string literal.
std::string escapeForDoubleQuoted(llvm::StringRef S) {
  std::string Result;
  Result.reserve(S.size());
  for (char C : S) {
    switch (C) {
    case '\\':
      Result += "\\\\";
      break;
    case '"':
      Result += "\\\"";
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
      Result += "\\$";
      break;
    default:
      Result += C;
    }
  }
  return Result;
}

/// \brief Escape a string for indented (multi-line) Nix string literal.
std::string escapeForIndented(llvm::StringRef S) {
  std::string Result;
  Result.reserve(S.size());
  for (size_t I = 0; I < S.size(); ++I) {
    char C = S[I];
    // In indented strings, '' followed by ' needs escaping as '''
    // and ${ needs escaping as ''${
    if (C == '$' && I + 1 < S.size() && S[I + 1] == '{') {
      Result += "''${";
      ++I; // Skip the {
    } else if (C == '\'' && I + 1 < S.size() && S[I + 1] == '\'') {
      Result += "'''";
      ++I; // Skip the second '
    } else {
      Result += C;
    }
  }
  return Result;
}

/// \brief Check if a string starts with '' (indented string literal).
bool isIndentedString(llvm::StringRef Src, const nixf::ExprString &Str) {
  size_t Start = Str.range().lCur().offset();
  if (Start + 1 >= Src.size())
    return false;
  return Src[Start] == '\'' && Src[Start + 1] == '\'';
}

/// \brief Add "Rewrite string" action to convert between string styles.
/// Example: "foo\nbar" -> ''foo
///   bar''
/// Example: ''foo
///   bar'' -> "foo\nbar"
void addRewriteStringAction(const nixf::Node &N,
                            const nixf::ParentMapAnalysis &PM,
                            const URIForFile &URI, llvm::StringRef Src,
                            std::vector<CodeAction> &Actions) {
  // Find if we're inside an ExprString
  const nixf::Node *StringNode = PM.upTo(N, nixf::Node::NK_ExprString);
  if (!StringNode)
    return;

  const auto &Str = static_cast<const nixf::ExprString &>(*StringNode);

  // Only work with literal strings (no interpolation)
  if (!Str.isLiteral())
    return;

  const std::string &Content = Str.literal();
  std::string FileURI = URI.uri();
  std::string NewText;
  std::string ActionTitle;

  if (isIndentedString(Src, Str)) {
    // Convert indented string to double-quoted
    NewText = "\"" + escapeForDoubleQuoted(Content) + "\"";
    ActionTitle = "Convert to double-quoted string";
  } else {
    // Convert double-quoted string to indented
    NewText = "''" + escapeForIndented(Content) + "''";
    ActionTitle = "Convert to indented string";
  }

  std::vector<lspserver::TextEdit> Edits;
  Edits.emplace_back(lspserver::TextEdit{
      .range = toLSPRange(Src, Str.range()),
      .newText = NewText,
  });

  using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
  WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};

  Actions.emplace_back(CodeAction{
      .title = ActionTitle,
      .kind = std::string(CodeAction::REFACTOR_REWRITE_KIND),
      .edit = std::move(WE),
  });
}

/// \brief Add "Convert to inherit" action for simple self-assignments.
/// Example: { x = x; } -> { inherit x; }
/// Example: { a = b.a; } -> { inherit (b) a; }
void addConvertToInheritAction(const nixf::Node &N,
                               const nixf::ParentMapAnalysis &PM,
                               const URIForFile &URI, llvm::StringRef Src,
                               std::vector<CodeAction> &Actions) {
  // Find if we're inside a Binding
  const nixf::Node *BindingNode = PM.upTo(N, nixf::Node::NK_Binding);
  if (!BindingNode)
    return;

  const auto &Bind = static_cast<const nixf::Binding &>(*BindingNode);

  // Path must be a single static name
  const auto &Names = Bind.path().names();
  if (Names.size() != 1 || !Names[0]->isStatic())
    return;

  const std::string &AttrName = Names[0]->staticName();

  // Check if value exists
  if (!Bind.value())
    return;

  const nixf::Expr *Value = Bind.value().get();

  std::string FileURI = URI.uri();

  // Case 1: Simple self-assignment { x = x; } -> { inherit x; }
  if (Value->kind() == nixf::Node::NK_ExprVar) {
    const auto &Var = static_cast<const nixf::ExprVar &>(*Value);
    if (Var.id().name() == AttrName) {
      std::string NewText = "inherit " + AttrName + ";";

      std::vector<lspserver::TextEdit> Edits;
      Edits.emplace_back(lspserver::TextEdit{
          .range = toLSPRange(Src, Bind.range()),
          .newText = NewText,
      });

      using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
      WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};

      Actions.emplace_back(CodeAction{
          .title = "Convert to inherit",
          .kind = std::string(CodeAction::REFACTOR_REWRITE_KIND),
          .edit = std::move(WE),
      });
    }
    return;
  }

  // Case 2: Select expression { a = b.a; } -> { inherit (b) a; }
  if (Value->kind() == nixf::Node::NK_ExprSelect) {
    const auto &Select = static_cast<const nixf::ExprSelect &>(*Value);

    // Must have a path with at least one element
    if (!Select.path() || Select.path()->names().empty())
      return;

    // Last element of path must match the attribute name
    const auto &PathNames = Select.path()->names();
    const auto &LastName = PathNames.back();
    if (!LastName->isStatic() || LastName->staticName() != AttrName)
      return;

    // Must not have a default expression
    if (Select.defaultExpr())
      return;

    // Build the inherit expression
    // Get the base expression (everything before the last selector)
    std::ostringstream NewText;
    NewText << "inherit (";

    // Get source of the base expression
    if (PathNames.size() == 1) {
      // Simple case: { a = b.a; } where Select.expr() is 'b'
      NewText << getSourceText(Src, Select.expr());
    } else {
      // Complex case: { a = b.c.a; } - need to build b.c
      NewText << getSourceText(Src, Select.expr());
      for (size_t I = 0; I + 1 < PathNames.size(); ++I) {
        NewText << "." << getSourceText(Src, *PathNames[I]);
      }
    }
    NewText << ") " << AttrName << ";";

    std::vector<lspserver::TextEdit> Edits;
    Edits.emplace_back(lspserver::TextEdit{
        .range = toLSPRange(Src, Bind.range()),
        .newText = NewText.str(),
    });

    using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
    WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};

    Actions.emplace_back(CodeAction{
        .title = "Convert to inherit",
        .kind = std::string(CodeAction::REFACTOR_REWRITE_KIND),
        .edit = std::move(WE),
    });
  }
}

/// \brief Add "Convert inherit to binding" action.
/// Example: { inherit x; } -> { x = x; }
/// Example: { inherit (b) a; } -> { a = b.a; }
void addInheritToBindingAction(const nixf::Node &N,
                               const nixf::ParentMapAnalysis &PM,
                               const URIForFile &URI, llvm::StringRef Src,
                               std::vector<CodeAction> &Actions) {
  // Find if we're inside an Inherit
  const nixf::Node *InheritNode = PM.upTo(N, nixf::Node::NK_Inherit);
  if (!InheritNode)
    return;

  const auto &Inherit = static_cast<const nixf::Inherit &>(*InheritNode);

  // Must have at least one name
  const auto &Names = Inherit.names();
  if (Names.empty())
    return;

  // Find which name the cursor is on
  const nixf::AttrName *TargetName = nullptr;
  for (const auto &Name : Names) {
    if (Name->range().lCur().offset() <= N.range().lCur().offset() &&
        N.range().rCur().offset() <= Name->range().rCur().offset()) {
      TargetName = Name.get();
      break;
    }
  }

  // If not on a specific name, use the first one
  if (!TargetName && !Names.empty())
    TargetName = Names[0].get();

  if (!TargetName || !TargetName->isStatic())
    return;

  const std::string &AttrName = TargetName->staticName();
  std::string FileURI = URI.uri();

  std::ostringstream NewText;
  NewText << AttrName << " = ";

  if (Inherit.expr()) {
    // inherit (expr) name; -> name = expr.name;
    NewText << getSourceText(Src, *Inherit.expr()) << "." << AttrName;
  } else {
    // inherit name; -> name = name;
    NewText << AttrName;
  }
  NewText << ";";

  // If this is the only name in inherit, replace the whole inherit
  // Otherwise, we'd need more complex handling (not implemented for simplicity)
  if (Names.size() != 1)
    return;

  std::vector<lspserver::TextEdit> Edits;
  Edits.emplace_back(lspserver::TextEdit{
      .range = toLSPRange(Src, Inherit.range()),
      .newText = NewText.str(),
  });

  using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
  WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};

  Actions.emplace_back(CodeAction{
      .title = "Convert to explicit binding",
      .kind = std::string(CodeAction::REFACTOR_REWRITE_KIND),
      .edit = std::move(WE),
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
    if (Result.Kind ==
        nixf::VariableLookupAnalysis::LookupResultKind::FromWith) {
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
      .kind = std::string(CodeAction::REFACTOR_REWRITE_KIND),
      .edit = std::move(WE),
  });
}

/// \brief Check if two ranges overlap (for conflict detection).
bool rangesOverlap(const lspserver::Range &A, const lspserver::Range &B) {
  // A ends before B starts, or B ends before A starts
  if (A.end.line < B.start.line ||
      (A.end.line == B.start.line && A.end.character <= B.start.character))
    return false;
  if (B.end.line < A.start.line ||
      (B.end.line == A.start.line && B.end.character <= A.start.character))
    return false;
  return true;
}

/// \brief Add source.fixAll action that applies all quickfixes.
void addFixAllAction(const std::vector<nixf::Diagnostic> &Diagnostics,
                     llvm::StringRef Src, const std::string &FileURI,
                     std::vector<CodeAction> &Actions) {
  std::vector<lspserver::TextEdit> AllEdits;
  std::vector<lspserver::Range> UsedRanges;

  // Collect all fixes from all diagnostics
  for (const nixf::Diagnostic &D : Diagnostics) {
    for (const nixf::Fix &F : D.fixes()) {
      bool HasConflict = false;

      // Check for conflicts with already collected edits
      for (const nixf::TextEdit &TE : F.edits()) {
        lspserver::Range EditRange = toLSPRange(Src, TE.oldRange());
        for (const auto &UsedRange : UsedRanges) {
          if (rangesOverlap(EditRange, UsedRange)) {
            HasConflict = true;
            break;
          }
        }
        if (HasConflict)
          break;
      }

      if (HasConflict)
        continue;

      // Add all edits from this fix
      for (const nixf::TextEdit &TE : F.edits()) {
        lspserver::Range EditRange = toLSPRange(Src, TE.oldRange());
        UsedRanges.push_back(EditRange);
        AllEdits.emplace_back(lspserver::TextEdit{
            .range = EditRange,
            .newText = std::string(TE.newText()),
        });
      }
    }
  }

  // Only add fixAll action if there are edits to apply
  if (AllEdits.empty())
    return;

  using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
  WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(AllEdits)}}};

  Actions.emplace_back(CodeAction{
      .title = "Fix all auto-fixable problems",
      .kind = std::string(CodeAction::SOURCE_FIXALL_KIND),
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

        // Determine if this diagnostic's fixes should be marked as preferred
        bool IsPreferred = false;
        switch (D.kind()) {
        case nixf::Diagnostic::DK_UndefinedVariable:
        case nixf::Diagnostic::DK_UnusedDefLet:
        case nixf::Diagnostic::DK_UnusedDefLambdaNoArg_Formal:
        case nixf::Diagnostic::DK_UnusedDefLambdaWithArg_Formal:
        case nixf::Diagnostic::DK_UnusedDefLambdaWithArg_Arg:
        case nixf::Diagnostic::DK_ExtraRecursive:
        case nixf::Diagnostic::DK_ExtraWith:
        case nixf::Diagnostic::DK_PrimOpNeedsPrefix:
        case nixf::Diagnostic::DK_EmptyInherit:
        case nixf::Diagnostic::DK_RedundantParen:
          IsPreferred = true;
          break;
        default:
          break;
        }

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
          using Changes =
              std::map<std::string, std::vector<lspserver::TextEdit>>;
          std::string FileURI = URIForFile::canonicalize(File, File).uri();
          WorkspaceEdit WE{.changes = Changes{
                               {std::move(FileURI), std::move(Edits)},
                           }};
          Actions.emplace_back(CodeAction{
              .title = F.message(),
              .kind = std::string(CodeAction::QUICKFIX_KIND),
              .isPreferred = IsPreferred,
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
          addConvertToInheritAction(*N, *TU->parentMap(), URI, TU->src(),
                                    Actions);
          addInheritToBindingAction(*N, *TU->parentMap(), URI, TU->src(),
                                    Actions);
          addRewriteStringAction(*N, *TU->parentMap(), URI, TU->src(), Actions);
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

      // Add source.fixAll action
      std::string FileURI = URIForFile::canonicalize(File, File).uri();
      addFixAllAction(Diagnostics, TU->src(), FileURI, Actions);

      return Actions;
    }());
  };
  boost::asio::post(Pool, std::move(Action));
}

} // namespace nixd
