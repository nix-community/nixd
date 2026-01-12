/// \file
/// \brief Implementation of with-to-let code action.

#include "WithToLet.h"
#include "Utils.h"

#include "../Convert.h"

#include <nixf/Basic/Nodes/Expr.h>
#include <nixf/Basic/Nodes/Simple.h>

#include <set>

namespace nixd {

namespace {

/// \brief Unwrap parenthesized expressions to get the inner expression.
/// For example, `(expr)` returns `expr`, `((expr))` returns `expr`.
const nixf::Expr *unwrapParen(const nixf::Expr *E) {
  while (E && E->kind() == nixf::Node::NK_ExprParen)
    E = static_cast<const nixf::ExprParen *>(E)->expr();
  return E;
}

/// \brief Check if the with body is directly another with expression.
/// This includes parenthesized with, e.g., `with a; (with b; x)`.
bool hasDirectlyNestedWith(const nixf::ExprWith &With) {
  const nixf::Expr *Body = unwrapParen(With.expr());
  return Body && Body->kind() == nixf::Node::NK_ExprWith;
}

/// \brief Check if cursor position is on the `with` keyword.
bool isCursorOnWithKeyword(const nixf::ExprWith &With, const nixf::Node &N) {
  // The node N should be or contain the `with` keyword
  const auto &KwWith = With.kwWith();
  auto KwRange = KwWith.range();
  auto NRange = N.range();

  // Check if N's range overlaps with the `with` keyword range
  return NRange.lCur().offset() <= KwRange.rCur().offset() &&
         NRange.rCur().offset() >= KwRange.lCur().offset();
}

/// \brief Collect unique variable names used from the with scope.
/// Returns empty set if the Definition cannot be obtained.
std::set<std::string>
collectWithVariables(const nixf::ExprWith &With,
                     const nixf::VariableLookupAnalysis &VLA) {
  std::set<std::string> VarNames;

  // Get the Definition for the `with` keyword
  const nixf::Definition *Def = VLA.toDef(With.kwWith());
  if (!Def)
    return VarNames;

  // Extract unique variable names from all uses
  for (const nixf::ExprVar *Var : Def->uses()) {
    if (Var)
      VarNames.insert(std::string(Var->id().name()));
  }

  return VarNames;
}

/// \brief Generate the let/inherit replacement text.
/// Format: let inherit (source) var1 var2 ...; in body
std::string generateLetInherit(const nixf::ExprWith &With,
                               const std::set<std::string> &VarNames,
                               llvm::StringRef Src) {
  std::string Result;

  // Get source expression text
  std::string_view SourceExpr;
  if (With.with())
    SourceExpr = With.with()->src(Src);
  else
    return ""; // Cannot generate without source expression

  // Get body expression text
  std::string_view BodyExpr;
  if (With.expr())
    BodyExpr = With.expr()->src(Src);
  else
    return ""; // Cannot generate without body expression

  // Build: let inherit (source) vars; in body
  Result.reserve(SourceExpr.size() + BodyExpr.size() + VarNames.size() * 10 +
                 30);

  Result += "let inherit (";
  Result += SourceExpr;
  Result += ")";

  // Add sorted variable names
  for (const auto &Name : VarNames) {
    Result += " ";
    Result += Name;
  }

  Result += "; in ";
  Result += BodyExpr;

  return Result;
}

} // namespace

void addWithToLetAction(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        const nixf::VariableLookupAnalysis &VLA,
                        const std::string &FileURI, llvm::StringRef Src,
                        std::vector<lspserver::CodeAction> &Actions) {
  // Find enclosing ExprWith node
  const nixf::Node *WithNode = PM.upTo(N, nixf::Node::NK_ExprWith);
  if (!WithNode)
    return;

  const auto &With = static_cast<const nixf::ExprWith &>(*WithNode);

  // Check if cursor is on the `with` keyword (not in the body or source expr)
  if (!isCursorOnWithKeyword(With, N))
    return;

  // Skip non-innermost with in nested chains to avoid semantic issues.
  // Converting outer `with` to `let/inherit` can change variable resolution
  // because `let` bindings shadow inner `with` scopes.
  // See: https://github.com/nix-community/nixd/pull/768#discussion_r2679465713
  if (hasDirectlyNestedWith(With))
    return;

  // Collect variables used from this with scope
  std::set<std::string> VarNames = collectWithVariables(With, VLA);

  // Skip if no variables are used (unused with)
  // The existing "remove with" quickfix handles this case
  if (VarNames.empty())
    return;

  // Generate the replacement text
  std::string NewText = generateLetInherit(With, VarNames, Src);
  if (NewText.empty())
    return;

  // Create the code action
  Actions.emplace_back(createSingleEditAction(
      "Convert `with` to `let/inherit`",
      lspserver::CodeAction::REFACTOR_REWRITE_KIND, FileURI,
      toLSPRange(Src, With.range()), std::move(NewText)));
}

} // namespace nixd
