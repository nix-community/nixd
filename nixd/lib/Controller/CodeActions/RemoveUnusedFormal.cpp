/// \file
/// \brief Implementation of "Remove unused formal" code action.
///
/// Removes unused formal parameters from the formals list of a lambda.
/// This is useful when a formal parameter is declared but never used
/// in the lambda body.

#include "RemoveUnusedFormal.h"

#include "../Convert.h"

#include <nixf/Basic/Nodes/Lambda.h>
#include <nixf/Sema/ParentMap.h>

namespace nixd {

void addRemoveUnusedFormalAction(
    const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
    const std::vector<nixf::Diagnostic> &Diagnostics,
    const std::string &FileURI, llvm::StringRef Src,
    std::vector<lspserver::CodeAction> &Actions) {

  // Find if cursor is on a Formal node
  const nixf::Node *FormalNode = PM.upTo(N, nixf::Node::NK_Formal);
  if (!FormalNode)
    return;

  const auto &Formal = static_cast<const nixf::Formal &>(*FormalNode);

  // Skip ellipsis formals - they can't be "unused"
  if (Formal.isEllipsis())
    return;

  // Check if there's an unused formal diagnostic at this position
  bool HasUnusedDiagnostic = false;
  for (const nixf::Diagnostic &D : Diagnostics) {
    if (D.kind() != nixf::Diagnostic::DK_UnusedDefLambdaNoArg_Formal &&
        D.kind() != nixf::Diagnostic::DK_UnusedDefLambdaWithArg_Formal) {
      continue;
    }

    // Check if the diagnostic range matches this formal's identifier
    if (Formal.id() && D.range().lCur() == Formal.id()->range().lCur() &&
        D.range().rCur() == Formal.id()->range().rCur()) {
      HasUnusedDiagnostic = true;
      break;
    }
  }

  if (!HasUnusedDiagnostic)
    return;

  // Find the enclosing Formals to get the FormalVector
  const nixf::Node *FormalsNode = PM.upTo(*FormalNode, nixf::Node::NK_Formals);
  if (!FormalsNode)
    return;

  const auto &Formals = static_cast<const nixf::Formals &>(*FormalsNode);
  const auto &Members = Formals.members();

  // Find the iterator for this formal
  nixf::Formals::FormalVector::const_iterator It;
  bool Found = false;
  for (It = Members.begin(); It != Members.end(); ++It) {
    if (It->get() == &Formal) {
      Found = true;
      break;
    }
  }

  if (!Found)
    return;

  // Generate edits to remove the formal
  std::vector<lspserver::TextEdit> Edits;

  // Remove the formal itself
  Edits.emplace_back(lspserver::TextEdit{
      .range = toLSPRange(Src, Formal.range()),
      .newText = "",
  });

  // Handle comma placement:
  // - If removing the first formal and there's a next formal, remove the next
  //   formal's leading comma
  // - If removing a non-first formal, the formal's own leading comma is part of
  //   its range (already removed above)
  if (It == Members.begin() && It + 1 != Members.end()) {
    const nixf::Formal &SecondF = **(It + 1);
    if (SecondF.comma()) {
      Edits.emplace_back(lspserver::TextEdit{
          .range = toLSPRange(Src, SecondF.comma()->range()),
          .newText = "",
      });
    }
  }

  // Get formal name for the title
  std::string FormalName =
      Formal.id() ? Formal.id()->name() : "<unknown>";
  std::string Title = "remove unused formal `" + FormalName + "`";

  using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
  lspserver::WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};

  lspserver::CodeAction Action;
  Action.title = std::move(Title);
  Action.kind = std::string(lspserver::CodeAction::QUICKFIX_KIND);
  Action.isPreferred = true;
  Action.edit = std::move(WE);

  Actions.emplace_back(std::move(Action));
}

} // namespace nixd
