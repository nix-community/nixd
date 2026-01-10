/// \file
/// \brief Implementation of extract-to-file code action.

#include "ExtractToFile.h"
#include "Utils.h"

#include "../Convert.h"

#include <nixf/Basic/Nodes/Attrs.h>
#include <nixf/Basic/Nodes/Expr.h>
#include <nixf/Basic/Nodes/Lambda.h>
#include <nixf/Basic/Nodes/Simple.h>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

#include <set>
#include <string_view>

namespace nixd {

namespace {

/// \brief Result of free variable collection.
struct FreeVariableResult {
  std::set<std::string> FreeVars;
  bool HasWithVars; ///< True if any variables come from 'with' scope
};

/// \brief Collect all free variables used within an expression subtree.
///
/// A free variable is one that is used (ExprVar) but defined outside the
/// expression being extracted. We traverse the subtree and collect variable
/// names that resolve to definitions outside our scope.
class FreeVariableCollector {
  const nixf::VariableLookupAnalysis &VLA;
  const nixf::Node &Root;
  std::set<std::string> FreeVars;
  bool HasWithVars = false;

  void collect(const nixf::Node &N) {
    // If this is a variable reference, check if it's free
    if (N.kind() == nixf::Node::NK_ExprVar) {
      const auto &Var = static_cast<const nixf::ExprVar &>(N);
      auto Result = VLA.query(Var);

      // Variable is free if:
      // - It's defined (not undefined/error)
      // - Its definition is outside our extraction root
      // - It's not a builtin
      if (Result.Kind ==
              nixf::VariableLookupAnalysis::LookupResultKind::Defined &&
          Result.Def && !Result.Def->isBuiltin()) {
        const nixf::Node *DefSyntax = Result.Def->syntax();
        if (DefSyntax && !isInsideNode(DefSyntax, Root)) {
          // This variable is defined outside our subtree - it's free
          FreeVars.insert(std::string(Var.id().name()));
        }
      }
      // Variables from 'with' are implicitly free since they depend on scope
      // Note: These may not work correctly after extraction since the 'with'
      // context is lost. We track this to warn the user.
      else if (Result.Kind ==
               nixf::VariableLookupAnalysis::LookupResultKind::FromWith) {
        FreeVars.insert(std::string(Var.id().name()));
        HasWithVars = true;
      }
    }

    // Recursively collect from children
    for (const auto &Child : N.children()) {
      if (Child)
        collect(*Child);
    }
  }

  /// \brief Check if a node is inside (or equal to) the root node.
  static bool isInsideNode(const nixf::Node *N, const nixf::Node &Root) {
    if (!N)
      return false;
    // Check if N's range is within Root's range
    const auto &NRange = N->range();
    const auto &RootRange = Root.range();
    return NRange.lCur().offset() >= RootRange.lCur().offset() &&
           NRange.rCur().offset() <= RootRange.rCur().offset();
  }

public:
  FreeVariableCollector(const nixf::VariableLookupAnalysis &VLA,
                        const nixf::Node &Root)
      : VLA(VLA), Root(Root) {}

  FreeVariableResult collect() {
    FreeVars.clear();
    HasWithVars = false;
    collect(Root);
    return {FreeVars, HasWithVars};
  }
};

/// \brief Generate a filename from the expression context.
///
/// Tries to derive a meaningful name from:
/// 1. If inside a binding, use the binding key name
/// 2. Otherwise, use a generic "extracted" name with expression type
std::string generateFilename(const nixf::Node &N,
                             const nixf::ParentMapAnalysis &PM) {
  // Check if we're inside a binding - use the binding key as filename
  const nixf::Node *BindingNode = PM.upTo(N, nixf::Node::NK_Binding);
  if (BindingNode) {
    const auto &Binding = static_cast<const nixf::Binding &>(*BindingNode);
    const auto &Names = Binding.path().names();
    if (!Names.empty() && Names.back()->isStatic()) {
      std::string Name = Names.back()->staticName();
      // Sanitize: replace invalid chars with underscore
      for (char &C : Name) {
        if (!std::isalnum(static_cast<unsigned char>(C)) && C != '-' &&
            C != '_') {
          C = '_';
        }
      }
      return Name + ".nix";
    }
  }

  // Fallback: use expression type
  switch (N.kind()) {
  case nixf::Node::NK_ExprLambda:
    return "extracted-lambda.nix";
  case nixf::Node::NK_ExprAttrs:
    return "extracted-attrs.nix";
  case nixf::Node::NK_ExprList:
    return "extracted-list.nix";
  case nixf::Node::NK_ExprLet:
    return "extracted-let.nix";
  case nixf::Node::NK_ExprIf:
    return "extracted-if.nix";
  default:
    return "extracted.nix";
  }
}

/// \brief Generate the content for the new file.
///
/// If there are free variables, wraps the expression in a lambda:
///   { freeVar1, freeVar2 }: <original expression>
std::string generateExtractedContent(llvm::StringRef ExprSrc,
                                     const std::set<std::string> &FreeVars) {
  std::string Content;

  if (!FreeVars.empty()) {
    // Generate lambda with formal arguments
    Content += "{ ";
    bool First = true;
    for (const auto &Var : FreeVars) {
      if (!First)
        Content += ", ";
      First = false;
      Content += Var;
    }
    Content += " }:\n";
  }

  Content += ExprSrc;
  Content += "\n";
  return Content;
}

/// \brief Generate the import statement to replace the original expression.
///
/// If there are free variables:
///   import ./filename.nix { inherit var1 var2; }
/// Otherwise:
///   import ./filename.nix
std::string generateImportStatement(const std::string &Filename,
                                    const std::set<std::string> &FreeVars) {
  std::string Import = "import ./";
  Import += Filename;

  if (!FreeVars.empty()) {
    Import += " { inherit";
    for (const auto &Var : FreeVars) {
      Import += " ";
      Import += Var;
    }
    Import += "; }";
  }

  return Import;
}

/// \brief Strip the "file://" scheme prefix from a URI if present.
///
/// LSP URIs typically have the form "file:///path/to/file", but
/// URIForFile::canonicalize expects a filesystem path without the scheme.
std::string stripFileScheme(llvm::StringRef URI) {
  if (URI.starts_with("file://"))
    return URI.drop_front(7).str();
  return URI.str();
}

/// \brief Generate a unique filename by appending a numeric suffix if needed.
///
/// If the file already exists, tries filename-1.nix, filename-2.nix, etc.
/// Returns the original filename if no conflict exists.
std::string makeUniqueFilename(llvm::StringRef Directory,
                               llvm::StringRef BaseFilename) {
  llvm::SmallString<256> TestPath(Directory);
  llvm::sys::path::append(TestPath, BaseFilename);

  if (!llvm::sys::fs::exists(TestPath))
    return std::string(BaseFilename);

  // Extract stem and extension
  llvm::StringRef Stem = llvm::sys::path::stem(BaseFilename);
  llvm::StringRef Ext = llvm::sys::path::extension(BaseFilename);

  // Try numbered variants
  constexpr int MaxAttempts = 100;
  for (int I = 1; I < MaxAttempts; ++I) {
    std::string Candidate = (Stem + "-" + std::to_string(I) + Ext).str();
    TestPath = Directory;
    llvm::sys::path::append(TestPath, Candidate);
    if (!llvm::sys::fs::exists(TestPath))
      return Candidate;
  }

  // Fallback: return original and let LSP client handle the error
  return std::string(BaseFilename);
}

/// \brief Check if an expression is suitable for extraction.
///
/// We allow extraction of any expression node, but skip trivial cases
/// like single identifiers or literals that wouldn't benefit from extraction.
/// Also skip empty structures and simple select expressions.
bool isExtractable(const nixf::Node &N) {
  switch (N.kind()) {
  // Skip trivial nodes that don't benefit from extraction
  case nixf::Node::NK_ExprVar:
  case nixf::Node::NK_ExprInt:
  case nixf::Node::NK_ExprFloat:
  case nixf::Node::NK_ExprString:
  case nixf::Node::NK_ExprPath:
  case nixf::Node::NK_ExprSPath:
  // Select expressions like `lib.foo` are just references - not worth
  // extracting
  case nixf::Node::NK_ExprSelect:
    return false;

  // For attribute sets, check if non-empty
  case nixf::Node::NK_ExprAttrs: {
    const auto &Attrs = static_cast<const nixf::ExprAttrs &>(N);
    // Empty attrsets {} are trivial
    const nixf::Binds *Binds = Attrs.binds();
    return Binds && !Binds->bindings().empty();
  }

  // For lists, check if non-empty
  case nixf::Node::NK_ExprList: {
    const auto &List = static_cast<const nixf::ExprList &>(N);
    // Empty lists [] are trivial
    return !List.elements().empty();
  }

  // Allow all other expression types
  case nixf::Node::NK_ExprLambda:
  case nixf::Node::NK_ExprLet:
  case nixf::Node::NK_ExprIf:
  case nixf::Node::NK_ExprWith:
  case nixf::Node::NK_ExprCall:
  case nixf::Node::NK_ExprBinOp:
  case nixf::Node::NK_ExprUnaryOp:
  case nixf::Node::NK_ExprOpHasAttr:
  case nixf::Node::NK_ExprAssert:
  case nixf::Node::NK_ExprParen:
    return true;

  default:
    return false;
  }
}

/// \brief Check if the given node is an expression node we can extract.
///
/// Only returns the node if it's directly extractable - does not walk up
/// the AST. This ensures we only offer extraction for the exact expression
/// under the cursor, not parent expressions.
const nixf::Node *findExtractableExpr(const nixf::Node &N,
                                      const nixf::ParentMapAnalysis &PM) {
  // Only offer extraction if this specific node is extractable.
  // Don't walk up - this prevents offering extraction for parent expressions
  // when the user has their cursor on a child (like an attribute name).
  if (isExtractable(N))
    return &N;

  // If the immediate node isn't extractable, check if we're inside a binding
  // value - allow extraction of the binding's value expression.
  // For example: { foo = { bar = 1; }; }
  //              cursor here ^-- should offer extraction of { bar = 1; }
  if (N.kind() == nixf::Node::NK_Identifier ||
      N.kind() == nixf::Node::NK_AttrName) {
    // We're on an attribute name/identifier, check if parent is AttrPath
    const nixf::Node *Parent = PM.query(N);
    if (Parent && Parent->kind() == nixf::Node::NK_AttrPath) {
      // Check if grandparent is Binding
      const nixf::Node *GrandParent = PM.query(*Parent);
      if (GrandParent && GrandParent->kind() == nixf::Node::NK_Binding) {
        const auto &Binding = static_cast<const nixf::Binding &>(*GrandParent);
        const auto &Value = Binding.value();
        if (Value && isExtractable(*Value))
          return Value.get();
      }
    }
  }

  return nullptr;
}

} // namespace

void addExtractToFileAction(const nixf::Node &N,
                            const nixf::ParentMapAnalysis &PM,
                            const nixf::VariableLookupAnalysis &VLA,
                            const std::string &FileURI, llvm::StringRef Src,
                            std::vector<lspserver::CodeAction> &Actions) {
  // Find an extractable expression at or above the cursor
  const nixf::Node *ExprNode = findExtractableExpr(N, PM);
  if (!ExprNode)
    return;

  // Get the expression source text
  std::string_view ExprSrc = ExprNode->src(Src);
  if (ExprSrc.empty())
    return;

  // Collect free variables
  FreeVariableCollector Collector(VLA, *ExprNode);
  FreeVariableResult FreeVarResult = Collector.collect();
  const std::set<std::string> &FreeVars = FreeVarResult.FreeVars;

  // Generate filename from context
  std::string BaseFilename = generateFilename(*ExprNode, PM);

  // Build the directory path for the new file (same directory as source)
  std::string SourceFilePath = stripFileScheme(FileURI);
  llvm::SmallString<256> Directory(SourceFilePath);
  llvm::sys::path::remove_filename(Directory);

  // Make filename unique if a file with the same name already exists
  std::string Filename = makeUniqueFilename(Directory, BaseFilename);

  // Generate the new file content
  std::string NewFileContent = generateExtractedContent(ExprSrc, FreeVars);

  // Generate the import statement
  std::string ImportStmt = generateImportStatement(Filename, FreeVars);

  // Build the full file path
  llvm::SmallString<256> NewFilePath(Directory);
  llvm::sys::path::append(NewFilePath, Filename);

  // Create the workspace edit with:
  // 1. CreateFile operation for the new file
  // 2. TextDocumentEdit to insert content into new file
  // 3. TextDocumentEdit to replace original expression with import

  lspserver::CreateFile CreateOp;
  CreateOp.uri = lspserver::URIForFile::canonicalize(std::string(NewFilePath),
                                                     SourceFilePath);
  CreateOp.options = lspserver::CreateFileOptions{};
  // Use overwrite=false (default) so the operation fails if file exists.
  // This prevents the inconsistent state where source is modified but
  // the target file already exists with different content.
  CreateOp.options->overwrite = false;
  CreateOp.options->ignoreIfExists = false;

  // Edit for new file: insert content at beginning
  lspserver::TextDocumentEdit NewFileEdit;
  NewFileEdit.textDocument.uri = CreateOp.uri;
  NewFileEdit.textDocument.version = 0;
  NewFileEdit.edits.push_back(lspserver::TextEdit{
      .range = lspserver::Range{{0, 0}, {0, 0}},
      .newText = NewFileContent,
  });

  // Edit for source file: replace expression with import
  lspserver::TextDocumentEdit SourceEdit;
  SourceEdit.textDocument.uri =
      lspserver::URIForFile::canonicalize(SourceFilePath, SourceFilePath);
  SourceEdit.edits.push_back(lspserver::TextEdit{
      .range = toLSPRange(Src, ExprNode->range()),
      .newText = ImportStmt,
  });

  // Build workspace edit with documentChanges (order matters!)
  lspserver::WorkspaceEdit WE;
  WE.documentChanges = std::vector<lspserver::DocumentChange>{};
  WE.documentChanges->push_back(CreateOp);
  WE.documentChanges->push_back(NewFileEdit);
  WE.documentChanges->push_back(SourceEdit);

  // Build action title
  std::string Title = "Extract to " + Filename;
  if (!FreeVars.empty()) {
    Title += " (";
    Title += std::to_string(FreeVars.size());
    Title += " free variable";
    if (FreeVars.size() > 1)
      Title += "s";
    // Warn about 'with' scope variables that may not work after extraction
    if (FreeVarResult.HasWithVars)
      Title += ", has 'with' vars";
    Title += ")";
  }

  lspserver::CodeAction Action;
  Action.title = std::move(Title);
  Action.kind = std::string(lspserver::CodeAction::REFACTOR_KIND);
  Action.edit = std::move(WE);

  Actions.push_back(std::move(Action));
}

} // namespace nixd
