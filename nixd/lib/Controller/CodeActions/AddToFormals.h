/// \file
/// \brief Code action for adding undefined variables to lambda formals.
///
/// Transforms: {x}: y -> {x, y}: y
/// This adds undefined variables to the formal parameter list of a lambda.

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

/// \brief Add code action to add undefined variable to lambda formals.
///
/// This action is offered when:
/// - The cursor is on an ExprVar (variable reference)
/// - The variable is undefined (not found in any scope)
/// - The enclosing lambda has a formals parameter list (e.g., {x, y}: body)
///
/// The action is NOT offered when:
/// - The variable is already defined in any scope
/// - There is no enclosing lambda
/// - The enclosing lambda uses simple argument style (x: body) instead of
/// formals
///
/// Transformation example: `{x}: y` -> `{x, y}: y`
void addToFormalsAction(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        const nixf::VariableLookupAnalysis &VLA,
                        const std::string &FileURI, llvm::StringRef Src,
                        std::vector<lspserver::CodeAction> &Actions);

} // namespace nixd
