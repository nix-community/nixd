/// \file
/// \brief This implements [Document Highlight].
/// [Document Highlight]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_documentHighlight

#include "Convert.h"
#include "Definition.h"

#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>
#include <lspserver/Protocol.h>
#include <nixf/Sema/ParentMap.h>
#include <nixf/Sema/VariableLookup.h>

using namespace lspserver;
using namespace nixd;
using namespace llvm;
using namespace nixf;

namespace {
Error highlight(const nixf::Node &Desc, const ParentMapAnalysis &PMA,
                const VariableLookupAnalysis &VLA, const URIForFile &URI,
                std::vector<DocumentHighlight> &Highlights) {
  // Find "definition"
  if (auto Def = findDefinition(Desc, PMA, VLA)) {
    // OK, iterate all uses.
    for (const auto *Use : Def->uses()) {
      assert(Use);
      Highlights.emplace_back(DocumentHighlight{
          .range = toLSPRange(Use->range()),
          .kind = DocumentHighlightKind::Read,
      });
    }
    if (Def->syntax()) {
      const Node &Syntax = *Def->syntax();
      Highlights.emplace_back(DocumentHighlight{
          .range = toLSPRange(Syntax.range()),
          .kind = DocumentHighlightKind::Write,
      });
    }
    return Error::success();
  }
  return error("Cannot find definition of this node");
}
} // namespace

void Controller::onDocumentHighlight(
    const TextDocumentPositionParams &Params,
    Callback<std::vector<DocumentHighlight>> Reply) {
  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 Pos = toNixfPosition(Params.position), this]() mutable {
    std::string File(URI.file());
    if (std::shared_ptr<NixTU> TU = getTU(File, Reply)) [[likely]] {
      if (std::shared_ptr<nixf::Node> AST = getAST(*TU, Reply)) [[likely]] {
        const nixf::Node *Desc = AST->descend({Pos, Pos});
        if (!Desc) {
          Reply(error("cannot find corresponding node on given position"));
          return;
        }
        std::vector<DocumentHighlight> Highlights;
        if (auto Err = highlight(*Desc, *TU->parentMap(), *TU->variableLookup(),
                                 URI, Highlights)) {
          // FIXME: Empty response if there are no def-use chain found.
          // For document highlights, the specification explicitly specified LSP
          // should do "fuzzy" things.

          // Empty response on error, don't reply all errors because this method
          // is very frequently called.
          Reply(std::vector<DocumentHighlight>{});
          lspserver::elog("textDocument/documentHighlight failed: {0}", Err);
          return;
        }
        Reply(std::move(Highlights));
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
