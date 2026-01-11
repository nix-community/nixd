/// \file
/// \brief Code action for converting `with` expressions to `let/inherit`.
///
/// Transforms: with lib; expr -> let inherit (lib) usedVars; in expr
/// This makes variable sources explicit and improves code clarity.

#pragma once

#include <lspserver/Protocol.h>

#include <nixf/Sema/ParentMap.h>
#include <nixf/Sema/VariableLookup.h>

#include <llvm/ADT/StringRef.h>

#include <string>
#include <vector>

namespace nixf {
class Node;
} // namespace nixf

namespace nixd {

/// \brief Add code action to convert `with` expression to `let/inherit`.
///
/// This action is offered when the cursor is on the `with` keyword.
/// It transforms `with source; body` into `let inherit (source) vars; in body`
/// where vars are all variables used from the with scope.
///
/// The action is NOT offered when:
/// - The cursor is not on the `with` keyword
/// - No variables are used from the with scope (unused with)
void addWithToLetAction(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        const nixf::VariableLookupAnalysis &VLA,
                        const std::string &FileURI, llvm::StringRef Src,
                        std::vector<lspserver::CodeAction> &Actions);

} // namespace nixd
