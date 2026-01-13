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

/// \brief Check if any variable used by this `with` could also be provided by
/// a nested (inner) `with` expression.
///
/// This uses semantic analysis to detect indirect nested `with` scenarios.
/// Converting an outer `with` to `let/inherit` is unsafe when variables could
/// come from inner `with` scopes, because `let` bindings shadow `with` scopes.
///
/// \param With The `with` expression to check.
/// \param VLA The variable lookup analysis containing scope information.
/// \return true if this `with` has nested `with` scopes that affect its
///         variables (conversion unsafe), false if it's safe to convert.
bool hasNestedWithScope(const nixf::ExprWith &With,
                        const nixf::VariableLookupAnalysis &VLA) {
  // Get the Definition for the `with` keyword to find all variables it provides
  const nixf::Definition *Def = VLA.toDef(With.kwWith());
  if (!Def)
    return false;

  // Check each variable used from this with scope
  for (const nixf::ExprVar *Var : Def->uses()) {
    if (!Var)
      continue;

    // Get all `with` scopes that could provide this variable
    auto WithScopes = VLA.getWithScopes(*Var);

    // If there are multiple `with` scopes, check if this `with` is not the
    // innermost one. The WithScopes vector is ordered innermost-to-outermost,
    // so if our `with` is not the first one, there's a nested `with`.
    if (WithScopes.size() > 1 && WithScopes.front() != &With) {
      // This variable could come from a different (inner) `with`,
      // so converting this outer `with` to `let/inherit` would change semantics
      return true;
    }
  }

  return false;
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

  // Skip `with` expressions that have nested `with` scopes (direct or indirect).
  // Converting such a `with` to `let/inherit` can change variable resolution
  // because `let` bindings shadow inner `with` scopes.
  // This semantic check handles both direct nesting (with a; with b; x) and
  // indirect nesting (with a; let y = with b; x; in y).
  // See: https://github.com/nix-community/nixd/pull/768#discussion_r2681198142
  if (hasNestedWithScope(With, VLA))
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
