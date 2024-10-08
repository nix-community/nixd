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

#include <exception>

using namespace lspserver;
using namespace nixd;
using namespace llvm;
using namespace nixf;

namespace {

std::vector<DocumentHighlight> highlight(const nixf::Node &Desc,
                                         const ParentMapAnalysis &PMA,
                                         const VariableLookupAnalysis &VLA,
                                         const URIForFile &URI,
                                         llvm::StringRef Src) {
  // Find "definition"
  auto Def = findDefinition(Desc, PMA, VLA);

  std::vector<DocumentHighlight> Highlights;
  // OK, iterate all uses.
  for (const auto *Use : Def.uses()) {
    assert(Use);
    Highlights.emplace_back(DocumentHighlight{
        .range = toLSPRange(Src, Use->range()),
        .kind = DocumentHighlightKind::Read,
    });
  }
  if (Def.syntax()) {
    const Node &Syntax = *Def.syntax();
    Highlights.emplace_back(DocumentHighlight{
        .range = toLSPRange(Src, Syntax.range()),
        .kind = DocumentHighlightKind::Write,
    });
  }

  return Highlights;
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
        try {
          const auto &PM = *TU->parentMap();
          const auto &VLA = *TU->variableLookup();
          return Reply(highlight(*Desc, PM, VLA, URI, TU->src()));
        } catch (std::exception &E) {
          elog("textDocument/documentHighlight failed: {0}", E.what());
          return Reply(std::vector<DocumentHighlight>{});
        }
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
