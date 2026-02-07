/// \file
/// \brief Implementation of attribute name quote/unquote code actions.

#include "AttrName.h"
#include "Utils.h"

#include "../Convert.h"

#include <nixf/Basic/Nodes/Attrs.h>

namespace nixd {

void addAttrNameActions(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        const std::string &FileURI, llvm::StringRef Src,
                        std::vector<lspserver::CodeAction> &Actions) {
  // Find if we're inside an AttrName
  const nixf::Node *AttrNameNode = PM.upTo(N, nixf::Node::NK_AttrName);
  if (!AttrNameNode)
    return;

  const auto &AN = static_cast<const nixf::AttrName &>(*AttrNameNode);

  if (AN.kind() == nixf::AttrName::ANK_ID) {
    // Offer to quote: foo -> "foo"
    const std::string &Name = AN.id()->name();
    Actions.emplace_back(createSingleEditAction(
        "Quote attribute name", lspserver::CodeAction::REFACTOR_REWRITE_KIND,
        FileURI, toLSPRange(Src, AN.range()), "\"" + Name + "\""));
  } else if (AN.kind() == nixf::AttrName::ANK_String && AN.isStatic()) {
    // Offer to unquote if valid identifier: "foo" -> foo
    const std::string &Name = AN.staticName();
    if (isValidNixIdentifier(Name)) {
      Actions.emplace_back(
          createSingleEditAction("Unquote attribute name",
                                 lspserver::CodeAction::REFACTOR_REWRITE_KIND,
                                 FileURI, toLSPRange(Src, AN.range()), Name));
    }
  }
}

} // namespace nixd
