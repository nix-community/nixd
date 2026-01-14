/// \file
/// \brief Implementation of inherit to explicit binding code action.

#include "InheritToBinding.h"
#include "Utils.h"

#include "../Convert.h"

#include <nixf/Basic/Nodes/Attrs.h>

#include <sstream>

namespace nixd {

void addInheritToBindingAction(const nixf::Node &N,
                               const nixf::ParentMapAnalysis &PM,
                               const std::string &FileURI, llvm::StringRef Src,
                               std::vector<lspserver::CodeAction> &Actions) {
  // Find if we're inside an Inherit node
  const nixf::Node *InheritNode = PM.upTo(N, nixf::Node::NK_Inherit);
  if (!InheritNode)
    return;

  const auto &Inherit = static_cast<const nixf::Inherit &>(*InheritNode);

  // Get the list of names in the inherit statement
  const auto &Names = Inherit.names();

  // Only handle single-name inherit statements
  // Multi-name inherits would require more complex handling
  if (Names.size() != 1)
    return;

  const auto &Name = Names[0];

  // Defensive null check
  if (!Name)
    return;

  // Only handle static names (not interpolated)
  if (!Name->isStatic())
    return;

  const std::string &AttrName = Name->staticName();

  // Build the replacement text
  std::ostringstream NewText;
  NewText << AttrName << " = ";

  if (Inherit.expr()) {
    // inherit (expr) name; -> name = expr.name;
    NewText << Inherit.expr()->src(Src) << "." << AttrName;
  } else {
    // inherit name; -> name = name;
    NewText << AttrName;
  }
  NewText << ";";

  // Create the code action
  Actions.emplace_back(createSingleEditAction(
      "Convert to explicit binding",
      lspserver::CodeAction::REFACTOR_REWRITE_KIND, FileURI,
      toLSPRange(Src, Inherit.range()), NewText.str()));
}

} // namespace nixd
