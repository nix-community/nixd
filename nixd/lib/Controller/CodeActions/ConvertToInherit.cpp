/// \file
/// \brief Implementation of convert to inherit code action.

#include "ConvertToInherit.h"
#include "Utils.h"

#include "../Convert.h"

#include <nixf/Basic/Nodes/Attrs.h>
#include <nixf/Basic/Nodes/Expr.h>
#include <nixf/Basic/Nodes/Simple.h>

namespace nixd {

void addConvertToInheritAction(const nixf::Node &N,
                               const nixf::ParentMapAnalysis &PM,
                               const std::string &FileURI, llvm::StringRef Src,
                               std::vector<lspserver::CodeAction> &Actions) {
  // Find if we're inside a Binding node
  const nixf::Node *BindingNode = PM.upTo(N, nixf::Node::NK_Binding);
  if (!BindingNode)
    return;

  const auto &Bind = static_cast<const nixf::Binding &>(*BindingNode);

  // Must have a single-segment path
  const auto &Names = Bind.path().names();
  if (Names.size() != 1)
    return;

  // Path must be static (not interpolated)
  const auto &AttrNameNode = Names[0];
  if (!AttrNameNode || !AttrNameNode->isStatic())
    return;

  const std::string &AttrName = AttrNameNode->staticName();

  // Must have a value
  if (!Bind.value())
    return;

  const nixf::Expr &Value = *Bind.value();

  // Case 1: { x = x; } -> { inherit x; }
  if (Value.kind() == nixf::Node::NK_ExprVar) {
    const auto &Var = static_cast<const nixf::ExprVar &>(Value);
    if (Var.id().name() == AttrName) {
      std::string NewText = "inherit " + quoteNixAttrKey(AttrName) + ";";
      Actions.emplace_back(createSingleEditAction(
          "Convert to `inherit`", lspserver::CodeAction::REFACTOR_REWRITE_KIND,
          FileURI, toLSPRange(Src, Bind.range()), std::move(NewText)));
    }
    return;
  }

  // Case 2: { a = source.a; } -> { inherit (source) a; }
  // Also handles { a = lib.nested.a; } -> { inherit (lib.nested) a; }
  if (Value.kind() == nixf::Node::NK_ExprSelect) {
    const auto &Sel = static_cast<const nixf::ExprSelect &>(Value);

    // Must not have a default value
    if (Sel.defaultExpr())
      return;

    // Must have a path
    if (!Sel.path())
      return;

    // Selector path must have at least one segment
    const auto &SelPath = Sel.path()->names();
    if (SelPath.empty())
      return;

    // The last segment must be static and match the attribute name
    const auto &LastSegment = SelPath.back();
    if (!LastSegment || !LastSegment->isStatic())
      return;

    if (LastSegment->staticName() != AttrName)
      return;

    // Build the source expression text:
    // - Start with the base expression (e.g., "lib" from lib.nested.x)
    // - Add any intermediate path segments (e.g., ".nested")
    const nixf::Expr &BaseExpr = Sel.expr();
    std::string SourceText =
        Src.slice(BaseExpr.lCur().offset(), BaseExpr.rCur().offset()).str();

    // Add intermediate path segments (all except the last one)
    for (size_t I = 0; I + 1 < SelPath.size(); ++I) {
      const auto &Segment = SelPath[I];
      if (!Segment || !Segment->isStatic())
        return; // Bail out if any intermediate segment is dynamic
      SourceText += ".";
      SourceText += quoteNixAttrKey(Segment->staticName());
    }

    std::string NewText =
        "inherit (" + SourceText + ") " + quoteNixAttrKey(AttrName) + ";";
    Actions.emplace_back(createSingleEditAction(
        "Convert to `inherit (" + SourceText + ")`",
        lspserver::CodeAction::REFACTOR_REWRITE_KIND, FileURI,
        toLSPRange(Src, Bind.range()), std::move(NewText)));
  }
}

} // namespace nixd
