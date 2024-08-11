/// \file
/// \brief This implements [Find References].
/// [Find References]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_references

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

std::vector<Location> findReferences(const nixf::Node &Desc,
                                     const ParentMapAnalysis &PMA,
                                     const VariableLookupAnalysis &VLA,
                                     const URIForFile &URI,
                                     llvm::StringRef Src) {

  // Two steps.
  //   1. Find some "definition" for this node.
  //   2. Find all "uses", and construct the vector.

  // Find "definition"
  auto Def = findDefinition(Desc, PMA, VLA);
  std::vector<Location> Locations;
  // OK, iterate all uses.
  for (const auto *Use : Def.uses()) {
    assert(Use);
    Locations.emplace_back(Location{
        .uri = URI,
        .range = toLSPRange(Src, Use->range()),
    });
  }
  return Locations;
}

} // namespace

void Controller::onReferences(const TextDocumentPositionParams &Params,
                              Callback<std::vector<Location>> Reply) {
  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 Pos = toNixfPosition(Params.position), this]() mutable {
    std::string File(URI.file());
    if (std::shared_ptr<NixTU> TU = getTU(File, Reply)) [[likely]] {
      if (std::shared_ptr<nixf::Node> AST = getAST(*TU, Reply)) [[likely]] {
        const nixf::Node *Desc = AST->descend({Pos, Pos});
        if (!Desc) {
          return Reply(
              error("cannot find corresponding node on given position"));
        }
        const auto &PM = *TU->parentMap();
        const auto &VLA = *TU->variableLookup();
        try {
          return Reply(findReferences(*Desc, PM, VLA, URI, TU->src()));
        } catch (std::exception &E) {
          return Reply(error("references: {0}", E.what()));
        }
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
