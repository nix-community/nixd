/// \file
/// \brief Code action for removing unused lambda formals.
///
/// Transforms: {x, y}: x -> {x}: x
/// This removes unused formal parameters from the lambda's formals list.

#pragma once

#include <llvm/ADT/StringRef.h>

#include <string>
#include <vector>

namespace lspserver {
struct CodeAction;
} // namespace lspserver

namespace nixf {
class Diagnostic;
class Node;
class ParentMapAnalysis;
} // namespace nixf

namespace nixd {

/// \brief Add code action to remove unused lambda formals.
///
/// This action is offered when:
/// - The cursor is on an unused formal parameter
/// - The diagnostic DK_UnusedDefLambdaNoArg_Formal or
///   DK_UnusedDefLambdaWithArg_Formal is present at the cursor position
///
/// Transformation example: `{x, y}: x` -> `{x}: x`
void addRemoveUnusedFormalAction(
    const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
    const std::vector<nixf::Diagnostic> &Diagnostics,
    const std::string &FileURI, llvm::StringRef Src,
    std::vector<lspserver::CodeAction> &Actions);

} // namespace nixd
