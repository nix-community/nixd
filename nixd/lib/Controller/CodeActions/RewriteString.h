/// \file
/// \brief Code action for converting between string literal syntaxes.
///
/// Transforms between double-quoted ("...") and indented (''...'') strings.

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

/// \brief Add rewrite action for string literal syntax conversion.
///
/// This action is offered when the cursor is on a string literal.
/// It converts between double-quoted strings ("...") and indented
/// strings (''...''), properly handling escape sequence differences.
void addRewriteStringAction(const nixf::Node &N,
                            const nixf::ParentMapAnalysis &PM,
                            const std::string &FileURI, llvm::StringRef Src,
                            std::vector<lspserver::CodeAction> &Actions);

} // namespace nixd
