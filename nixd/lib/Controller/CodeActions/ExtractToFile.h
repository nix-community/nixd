/// \file
/// \brief Code action for extracting expressions to separate files.
///
/// Allows users to select any Nix expression and extract it to a new file,
/// automatically generating an import statement with appropriate argument
/// passing for free variables.

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

/// \brief Add extract-to-file action for selected expressions.
///
/// This action is offered when the cursor is on any valid Nix expression.
/// It creates a new file with the expression content and replaces the original
/// expression with an import statement. If the expression contains free
/// variables (variables used but not defined within the expression), they are
/// wrapped in a lambda in the new file.
///
/// Example transformation:
///   Original: { buildInputs = [ pkgs.foo pkgs.bar ]; }
///   New file (buildInputs.nix): { pkgs }: [ pkgs.foo pkgs.bar ]
///   Replaced: { buildInputs = import ./buildInputs.nix { inherit pkgs; }; }
void addExtractToFileAction(const nixf::Node &N,
                            const nixf::ParentMapAnalysis &PM,
                            const nixf::VariableLookupAnalysis &VLA,
                            const std::string &FileURI, llvm::StringRef Src,
                            std::vector<lspserver::CodeAction> &Actions);

} // namespace nixd
