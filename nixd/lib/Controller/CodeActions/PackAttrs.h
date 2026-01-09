/// \file
/// \brief Code action for packing dotted attribute paths into nested sets.
///
/// Transforms: { foo.bar = 1; } -> { foo = { bar = 1; }; }
/// Also offers bulk pack when siblings share a prefix:
/// { foo.bar = 1; foo.baz = 2; } -> { foo = { bar = 1; baz = 2; }; }

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

/// \brief Add pack action for dotted attribute paths.
///
/// This action is offered when the cursor is on a binding with a dotted path
/// (e.g., foo.bar = 1). It offers three variants:
/// - Pack One: pack only the current binding
/// - Shallow Pack All: pack all siblings sharing the same first segment
/// - Recursive Pack All: fully nest all siblings
void addPackAttrsAction(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        const std::string &FileURI, llvm::StringRef Src,
                        std::vector<lspserver::CodeAction> &Actions);

} // namespace nixd
