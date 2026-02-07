/// \file
/// \brief Code action for converting JSON to Nix expressions.
///
/// Provides a selection-based code action that converts valid JSON text
/// to equivalent Nix expression syntax.

#pragma once

#include <lspserver/Protocol.h>

#include <llvm/ADT/StringRef.h>

#include <string>
#include <vector>

namespace nixd {

/// \brief Add JSON to Nix conversion action for selected JSON text.
///
/// This is a selection-based action that works on arbitrary text, not AST
/// nodes. It parses the selected text as JSON and converts it to Nix syntax.
void addJsonToNixAction(llvm::StringRef Src, const lspserver::Range &Range,
                        const std::string &FileURI,
                        std::vector<lspserver::CodeAction> &Actions);

} // namespace nixd
