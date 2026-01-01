/// \file
/// \brief Implementation of [Folding Range].
/// [Folding Range]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_foldingRange

#include "CheckReturn.h"
#include "Convert.h"

#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>
#include <lspserver/Logger.h>
#include <lspserver/Protocol.h>
#include <nixf/Basic/Nodes/Attrs.h>
#include <nixf/Basic/Nodes/Expr.h>
#include <nixf/Basic/Nodes/Lambda.h>
#include <nixf/Basic/Nodes/Simple.h>

using namespace nixd;
using namespace lspserver;
using namespace nixf;

namespace {

/// Maximum recursion depth to prevent stack overflow on deeply nested ASTs.
constexpr size_t MaxRecursionDepth = 256;

/// Check if the range spans multiple lines (2+ lines required for folding).
bool isMultiLine(const lspserver::Range &R) { return R.start.line < R.end.line; }

/// Create a FoldingRange from an LSP Range with "region" kind.
FoldingRange toFoldingRange(const lspserver::Range &R) {
  FoldingRange FR;
  FR.startLine = R.start.line;
  FR.startCharacter = R.start.character;
  FR.endLine = R.end.line;
  FR.endCharacter = R.end.character;
  FR.kind = FoldingRange::REGION_KIND;
  return FR;
}

/// Add a folding range if the node spans multiple lines.
void addFoldingRange(const Node &N, std::vector<FoldingRange> &Ranges,
                     llvm::StringRef Src) {
  auto R = toLSPRange(Src, N.range());
  if (isMultiLine(R))
    Ranges.emplace_back(toFoldingRange(R));
}

/// Collect folding ranges from AST nodes recursively.
/// \param AST The AST node to process
/// \param Ranges Output vector to collect folding ranges
/// \param Src Source text for position conversion
/// \param Depth Current recursion depth (for stack overflow protection)
void collectFoldingRanges(const Node *AST, std::vector<FoldingRange> &Ranges,
                          llvm::StringRef Src, size_t Depth = 0) {
  if (!AST || Depth >= MaxRecursionDepth)
    return;

  switch (AST->kind()) {
  case Node::NK_ExprAttrs: {
    addFoldingRange(*AST, Ranges, Src);
    const auto &Attrs = static_cast<const ExprAttrs &>(*AST);
    if (Attrs.binds()) {
      for (const Node *Ch : Attrs.binds()->children())
        collectFoldingRanges(Ch, Ranges, Src, Depth + 1);
    }
    break;
  }
  case Node::NK_ExprList: {
    addFoldingRange(*AST, Ranges, Src);
    for (const Node *Ch : AST->children())
      collectFoldingRanges(Ch, Ranges, Src, Depth + 1);
    break;
  }
  case Node::NK_ExprLambda: {
    addFoldingRange(*AST, Ranges, Src);
    const auto &Lambda = static_cast<const ExprLambda &>(*AST);
    if (const auto *Body = Lambda.body())
      collectFoldingRanges(Body, Ranges, Src, Depth + 1);
    break;
  }
  case Node::NK_ExprLet: {
    addFoldingRange(*AST, Ranges, Src);
    const auto &Let = static_cast<const ExprLet &>(*AST);
    if (const auto *Attrs = Let.attrs())
      collectFoldingRanges(Attrs, Ranges, Src, Depth + 1);
    if (const auto *E = Let.expr())
      collectFoldingRanges(E, Ranges, Src, Depth + 1);
    break;
  }
  case Node::NK_ExprWith: {
    addFoldingRange(*AST, Ranges, Src);
    const auto &With = static_cast<const ExprWith &>(*AST);
    if (const auto *W = With.with())
      collectFoldingRanges(W, Ranges, Src, Depth + 1);
    if (const auto *E = With.expr())
      collectFoldingRanges(E, Ranges, Src, Depth + 1);
    break;
  }
  case Node::NK_ExprIf: {
    addFoldingRange(*AST, Ranges, Src);
    const auto &If = static_cast<const ExprIf &>(*AST);
    if (const auto *Cond = If.cond())
      collectFoldingRanges(Cond, Ranges, Src, Depth + 1);
    if (const auto *Then = If.then())
      collectFoldingRanges(Then, Ranges, Src, Depth + 1);
    if (const auto *Else = If.elseExpr())
      collectFoldingRanges(Else, Ranges, Src, Depth + 1);
    break;
  }
  case Node::NK_ExprString:
    // Multiline strings are foldable regions
    addFoldingRange(*AST, Ranges, Src);
    break;
  default:
    // Other node types may contain foldable children, recurse into them
    for (const Node *Ch : AST->children())
      collectFoldingRanges(Ch, Ranges, Src, Depth + 1);
    break;
  }
}

} // namespace

/// \brief Handle textDocument/foldingRange LSP request.
///
/// Collects foldable regions from the document's AST including attribute sets,
/// lists, lambdas, let/with/if expressions, and multiline strings.
///
/// \param Params Request parameters containing the document URI
/// \param Reply Callback to return the list of folding ranges
void Controller::onFoldingRange(
    const FoldingRangeParams &Params,
    Callback<std::vector<FoldingRange>> Reply) {
  using CheckTy = std::vector<FoldingRange>;
  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 this]() mutable {
    return Reply([&]() -> llvm::Expected<CheckTy> {
      const auto TU = CheckDefault(getTU(URI.file().str()));
      const auto AST = CheckDefault(getAST(*TU));
      try {
        auto Ranges = std::vector<FoldingRange>();
        collectFoldingRanges(AST.get(), Ranges, TU->src());
        return Ranges;
      } catch (std::exception &E) {
        elog("textDocument/foldingRange failed: {0}", E.what());
        return CheckTy{};
      }
    }());
  };
  boost::asio::post(Pool, std::move(Action));
}
