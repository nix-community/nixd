/// \file
/// \brief Implementation of inherit to explicit binding code action.

#include "InheritToBinding.h"
#include "Utils.h"

#include "../Convert.h"

#include <nixf/Basic/Nodes/Attrs.h>
#include <nixf/Basic/Nodes/Expr.h>

#include <algorithm>

namespace nixd {

namespace {

/// The text of \p E as the base of a selection `E.name`. Selection binds
/// tighter than everything but primary expressions, and a path or URI would
/// take `.name` as part of itself, so anything else is parenthesised.
std::string selectBase(const nixf::Expr &E, llvm::StringRef Src) {
  std::string Text(E.src(Src));
  switch (E.kind()) {
  case nixf::Node::NK_ExprVar:
  case nixf::Node::NK_ExprParen:
  case nixf::Node::NK_ExprAttrs:
  case nixf::Node::NK_ExprList:
  case nixf::Node::NK_ExprSPath:
    return Text;
  case nixf::Node::NK_ExprSelect:
    // `a.b or c.name` is `a.b or (c.name)`.
    if (!static_cast<const nixf::ExprSelect &>(E).defaultExpr())
      return Text;
    break;
  case nixf::Node::NK_ExprString:
    // A URI is desugared into a string.
    if (llvm::StringRef(Text).starts_with("\"") ||
        llvm::StringRef(Text).starts_with("''"))
      return Text;
    break;
  default:
    break;
  }
  return "(" + Text + ")";
}

} // namespace

void addInheritToBindingAction(const nixf::Node &N,
                               const nixf::ParentMapAnalysis &PM,
                               const std::string &FileURI, llvm::StringRef Src,
                               std::vector<lspserver::CodeAction> &Actions) {
  // Find if we're inside an Inherit node
  const nixf::Node *InheritNode = PM.upTo(N, nixf::Node::NK_Inherit);
  if (!InheritNode)
    return;

  const auto &Inherit = static_cast<const nixf::Inherit &>(*InheritNode);

  const auto &Names = Inherit.names();

  // With several names, convert the one under the cursor: the others stay
  // inherited, and the source expression is copied once, not once per name.
  const nixf::AttrName *Name = nullptr;
  if (Names.size() == 1) {
    Name = Names[0].get();
  } else if (const nixf::Node *Up = PM.upTo(N, nixf::Node::NK_AttrName)) {
    for (const auto &Each : Names)
      if (Each.get() == Up)
        Name = Each.get();
  }
  if (!Name || !Name->isStatic())
    return;

  const std::string &Static = Name->staticName();
  std::string Value;
  if (const nixf::Expr *E = Inherit.expr().get()) {
    // inherit (expr) name; -> name = expr.name;
    Value = selectBase(*E, Src) + "." + quoteNixAttrKey(Static);
  } else {
    // inherit name; -> name = name; but in a `rec` or `let`, where `inherit`
    // reads `name` from outside, `name = name;` would read itself.
    const nixf::Node *Attrs = PM.upTo(Inherit, nixf::Node::NK_ExprAttrs);
    if (!Attrs || !isValidNixIdentifier(Static) ||
        static_cast<const nixf::ExprAttrs &>(*Attrs).sema().isRecursive())
      return;
    Value = Static;
  }
  std::string Binding = quoteNixAttrKey(Static) + " = " + Value + ";";

  if (Names.size() == 1) {
    Actions.emplace_back(createSingleEditAction(
        "Convert to explicit binding",
        lspserver::CodeAction::REFACTOR_REWRITE_KIND, FileURI,
        toLSPRange(Src, Inherit.range()), std::move(Binding)));
    return;
  }

  // Remove the name with the space that separates it from its neighbour, and
  // add the binding after the inherit.
  auto It = std::find_if(Names.begin(), Names.end(),
                         [&](const auto &Each) { return Each.get() == Name; });
  nixf::LexerCursorRange Removal =
      std::next(It) != Names.end()
          ? nixf::LexerCursorRange{Name->lCur(), (*std::next(It))->lCur()}
          : nixf::LexerCursorRange{(*std::prev(It))->rCur(), Name->rCur()};
  std::vector<lspserver::TextEdit> Edits{
      {.range = toLSPRange(Src, Removal), .newText = ""},
      {.range = toLSPRange(Src, nixf::LexerCursorRange{Inherit.rCur()}),
       .newText = " " + std::move(Binding)},
  };
  Actions.emplace_back(createMultiEditAction(
      "Convert to explicit binding",
      lspserver::CodeAction::REFACTOR_REWRITE_KIND, FileURI, std::move(Edits)));
}

} // namespace nixd
