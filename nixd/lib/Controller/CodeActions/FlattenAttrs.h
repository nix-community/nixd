/// \file
/// \brief Code action for flattening nested attribute sets.
///
/// Transforms: { foo = { bar = 1; }; } -> { foo.bar = 1; }

#pragma once

#include <lspserver/Protocol.h>

#include <nixf/Sema/ParentMap.h>

#include <llvm/ADT/StringRef.h>

#include <string>
#include <vector>

namespace nixf {
class Node;
} // namespace nixf

namespace nixd {

/// \brief Add flatten action for nested attribute sets.
///
/// This action is offered when the cursor is on a binding whose value is
/// a non-recursive attribute set. It flattens all nested bindings into
/// dotted paths.
void addFlattenAttrsAction(const nixf::Node &N,
                           const nixf::ParentMapAnalysis &PM,
                           const std::string &FileURI, llvm::StringRef Src,
                           std::vector<lspserver::CodeAction> &Actions);

} // namespace nixd
