/// \file
/// \brief Implementation of "Add to formals" code action.
///
/// Adds undefined variables to the formals of the enclosing lambda expression.
/// This is useful when a variable is used in a lambda body but not declared
/// in the formals list.

#include "AddToFormals.h"
#include "Utils.h"

#include "../Convert.h"

#include <nixf/Basic/Nodes/Lambda.h>
#include <nixf/Basic/Nodes/Simple.h>
#include <nixf/Sema/ParentMap.h>
#include <nixf/Sema/VariableLookup.h>

namespace nixd {

void addToFormalsAction(const nixf::Node &N, const nixf::ParentMapAnalysis &PM,
                        const nixf::VariableLookupAnalysis &VLA,
                        const std::string &FileURI, llvm::StringRef Src,
                        std::vector<lspserver::CodeAction> &Actions) {
  // Find ExprVar node (cursor might be on Identifier child)
  const nixf::Node *VarNode = PM.upTo(N, nixf::Node::NK_ExprVar);
  if (!VarNode)
    return;
  const auto &Var = static_cast<const nixf::ExprVar &>(*VarNode);

  // Check if variable is undefined using VLA
  auto Result = VLA.query(Var);
  if (Result.Kind != nixf::VariableLookupAnalysis::LookupResultKind::Undefined)
    return;

  // Find enclosing lambda using PM.upTo()
  const nixf::Node *LambdaNode = PM.upTo(N, nixf::Node::NK_ExprLambda);
  if (!LambdaNode)
    return;

  const auto &Lambda = static_cast<const nixf::ExprLambda &>(*LambdaNode);

  // Check if lambda has formals (not simple `x: body` style)
  if (!Lambda.arg() || !Lambda.arg()->formals())
    return;

  const nixf::Formals &Formals = *Lambda.arg()->formals();
  const std::string &VarName = Var.id().name();

  // Check variable doesn't already exist in formals
  if (Formals.dedup().contains(VarName))
    return;

  // Determine insertion point
  // There are 4 cases to handle:
  // 1. Empty formals `{ }:` -> insert varName after `{`
  // 2. Normal `{ a }:` -> insert `, varName` after last formal
  // 3. Ellipsis only `{ ... }:` -> insert `varName, ` before `...`
  // 4. With ellipsis `{ a, ... }:` -> insert `, varName` after last
  // non-ellipsis formal

  std::string NewText;
  lspserver::Range InsertRange;

  const auto &Members = Formals.members();

  if (Members.empty()) {
    // Case 1: Empty formals `{ }:`
    // Insert right after the opening brace `{`
    size_t BraceOffset = Formals.lCur().offset();
    size_t InsertOffset = BraceOffset + 1;
    auto InsertCursor = nixf::LexerCursor::unsafeCreate(
        Formals.lCur().line(), Formals.lCur().column() + 1, InsertOffset);
    auto InsertPos = toLSPPosition(Src, InsertCursor);
    InsertRange = lspserver::Range{InsertPos, InsertPos};
    NewText = " " + quoteNixAttrKey(VarName) + " ";
  } else {
    // Check if the last member is an ellipsis
    const nixf::Formal *LastMember = Members.back().get();
    bool HasEllipsis = LastMember && LastMember->isEllipsis();

    if (HasEllipsis) {
      if (Members.size() == 1) {
        // Case 3: Ellipsis only `{ ... }:`
        // Insert `varName, ` before the ellipsis
        auto InsertPos = toLSPPosition(Src, LastMember->lCur());
        InsertRange = lspserver::Range{InsertPos, InsertPos};
        NewText = quoteNixAttrKey(VarName) + ", ";
      } else {
        // Case 4: With ellipsis `{ a, ... }:`
        // Insert `, varName` after the last non-ellipsis formal
        // Find the last non-ellipsis formal
        const nixf::Formal *LastNonEllipsis = nullptr;
        for (auto It = Members.rbegin(); It != Members.rend(); ++It) {
          if ((*It) && !(*It)->isEllipsis()) {
            LastNonEllipsis = (*It).get();
            break;
          }
        }

        if (!LastNonEllipsis)
          return;

        auto InsertPos = toLSPPosition(Src, LastNonEllipsis->rCur());
        InsertRange = lspserver::Range{InsertPos, InsertPos};
        NewText = ", " + quoteNixAttrKey(VarName);
      }
    } else {
      // Case 2: Normal `{ a }:` without ellipsis
      // Insert `, varName` after the last formal
      auto InsertPos = toLSPPosition(Src, LastMember->rCur());
      InsertRange = lspserver::Range{InsertPos, InsertPos};
      NewText = ", " + quoteNixAttrKey(VarName);
    }
  }

  // Create CodeAction
  std::string Title = "add `" + VarName + "` to formals";

  std::vector<lspserver::TextEdit> Edits;
  Edits.emplace_back(
      lspserver::TextEdit{.range = InsertRange, .newText = std::move(NewText)});

  using Changes = std::map<std::string, std::vector<lspserver::TextEdit>>;
  lspserver::WorkspaceEdit WE{.changes = Changes{{FileURI, std::move(Edits)}}};

  lspserver::CodeAction Action;
  Action.title = std::move(Title);
  Action.kind = std::string(lspserver::CodeAction::QUICKFIX_KIND);
  Action.isPreferred = true;
  Action.edit = std::move(WE);

  Actions.emplace_back(std::move(Action));
}

} // namespace nixd
