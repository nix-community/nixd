/// \file
/// \brief Implementation of noogle.dev documentation code action.

#include "NoogleDoc.h"

#include <nixf/Basic/Nodes/Expr.h>

#include <llvm/Support/JSON.h>

namespace nixd {

namespace {

/// \brief Construct noogle.dev URL for a lib.* function path.
/// Examples:
///   - {"lib", "optionalString"} -> "https://noogle.dev/f/lib/optionalString"
///   - {"lib", "strings", "optionalString"} ->
///   "https://noogle.dev/f/lib/strings/optionalString"
std::string buildNoogleUrl(const std::vector<std::string> &Path) {
  std::string Url = "https://noogle.dev/f";
  for (const auto &Segment : Path) {
    Url += "/";
    Url += Segment;
  }
  return Url;
}

} // namespace

void addNoogleDocAction(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        std::vector<lspserver::CodeAction> &Actions) {
  // Find if we're inside an ExprSelect
  const nixf::Node *SelectNode = PM.upTo(N, nixf::Node::NK_ExprSelect);
  if (!SelectNode)
    return;

  const auto &Sel = static_cast<const nixf::ExprSelect &>(*SelectNode);

  // Check base expression is ExprVar with name "lib"
  if (Sel.expr().kind() != nixf::Node::NK_ExprVar)
    return;

  const auto &Var = static_cast<const nixf::ExprVar &>(Sel.expr());
  if (Var.id().name() != "lib")
    return;

  // Check path exists and has at least one attribute
  if (!Sel.path())
    return;

  const nixf::AttrPath &Path = *Sel.path();
  if (Path.names().empty())
    return;

  // Build the function path, checking all names are static
  std::vector<std::string> FunctionPath;
  FunctionPath.reserve(Path.names().size() + 1);
  FunctionPath.emplace_back("lib");

  for (const auto &Name : Path.names()) {
    if (!Name->isStatic())
      return; // Dynamic attribute, can't construct URL
    FunctionPath.emplace_back(Name->staticName());
  }

  // Construct the noogle.dev URL
  std::string NoogleUrl = buildNoogleUrl(FunctionPath);

  // Create a code action that will open the URL
  // Note: The actual URL opening is handled by the client via
  // window/showDocument
  Actions.emplace_back(lspserver::CodeAction{
      .title = "Open Noogle documentation for " + FunctionPath.back(),
      .kind = std::string(lspserver::CodeAction::REFACTOR_KIND),
      .data = llvm::json::Object{{"noogleUrl", NoogleUrl}},
  });
}

} // namespace nixd
