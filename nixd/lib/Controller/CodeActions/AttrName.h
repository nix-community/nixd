/// \file
/// \brief Code action for quoting/unquoting attribute names.
///
/// Offers to quote an unquoted attribute name (foo -> "foo") or
/// unquote a quoted attribute name if it's a valid identifier ("foo" -> foo).

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

/// \brief Add refactoring code actions for attribute names (quote/unquote).
void addAttrNameActions(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        const std::string &FileURI, llvm::StringRef Src,
                        std::vector<lspserver::CodeAction> &Actions);

} // namespace nixd
