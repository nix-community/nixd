/// \file
/// \brief Code action for opening noogle.dev documentation.
///
/// Provides a code action to open noogle.dev documentation for lib.* functions.

#pragma once

#include <lspserver/Protocol.h>

#include <nixf/Sema/ParentMap.h>

#include <string>
#include <vector>

namespace nixf {
class Node;
} // namespace nixf

namespace nixd {

/// \brief Add a code action to open noogle.dev documentation for lib.*
/// functions.
///
/// This action is offered when the cursor is on an ExprSelect with:
///   - Base expression is ExprVar with name "lib"
///   - Path contains at least one static attribute name
///
/// Examples that trigger:
///   - lib.optionalString
///   - lib.strings.optionalString
///   - lib.attrsets.mapAttrs
///
/// Examples that do NOT trigger:
///   - lib (just the variable, no selection)
///   - lib.${x} (dynamic attribute)
///   - pkgs.hello (not lib.*)
void addNoogleDocAction(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        std::vector<lspserver::CodeAction> &Actions);

} // namespace nixd
