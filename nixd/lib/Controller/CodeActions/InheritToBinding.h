/// \file
/// \brief Code action for converting inherit to explicit binding.
///
/// Offers to convert inherit statements to explicit bindings:
/// - { inherit x; } -> { x = x; }
/// - { inherit (b) a; } -> { a = b.a; }

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

/// \brief Add code action to convert inherit to explicit binding.
///
/// This action converts inherit statements to explicit bindings:
/// - Simple inherit: { inherit x; } -> { x = x; }
/// - Inherit from expression: { inherit (b) a; } -> { a = b.a; }
///
/// The action is only offered when the inherit has exactly one name.
void addInheritToBindingAction(const nixf::Node &N,
                               const nixf::ParentMapAnalysis &PM,
                               const std::string &FileURI, llvm::StringRef Src,
                               std::vector<lspserver::CodeAction> &Actions);

} // namespace nixd
