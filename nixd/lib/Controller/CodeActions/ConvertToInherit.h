/// \file
/// \brief Code action for converting bindings to inherit syntax.
///
/// Transforms:
/// - { x = x; } -> { inherit x; }
/// - { a = lib.a; } -> { inherit (lib) a; }

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

/// \brief Add code action to convert binding to inherit syntax.
///
/// This action is offered when the cursor is on a binding that matches
/// one of these patterns:
/// - { attrName = attrName; } -> { inherit attrName; }
/// - { attrName = source.attrName; } -> { inherit (source) attrName; }
///
/// The action is NOT offered when:
/// - The binding has a multi-segment path (e.g., x.y = z)
/// - The attribute name is dynamic/interpolated
/// - The value doesn't match the attribute name
/// - The select expression has a default value
void addConvertToInheritAction(const nixf::Node &N,
                               const nixf::ParentMapAnalysis &PM,
                               const std::string &FileURI, llvm::StringRef Src,
                               std::vector<lspserver::CodeAction> &Actions);

} // namespace nixd
