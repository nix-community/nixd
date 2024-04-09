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

Error findReferences(const nixf::Node &Desc, const ParentMapAnalysis &PMA,
                     const VariableLookupAnalysis &VLA, const URIForFile &URI,
                     std::vector<Location> &Locations) {

  // Two steps.
  //   1. Find some "definition" for this node.
  //   2. Find all "uses", and construct the vector.

  // Find "definition"
  if (auto Def = findDefinition(Desc, PMA, VLA)) {
    // OK, iterate all uses.
    for (const auto *Use : Def->uses()) {
      assert(Use);
      Locations.emplace_back(Location{
          .uri = URI,
          .range = toLSPRange(Use->range()),
      });
    }
    return Error::success();
  }
  return error("Cannot find definition of this node");
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
          Reply(error("cannot find corresponding node on given position"));
          return;
        }
        std::vector<Location> Locations;
        if (auto Err = findReferences(*Desc, *TU->parentMap(),
                                      *TU->variableLookup(), URI, Locations)) {
          Reply(std::move(Err));
          return;
        }
        Reply(std::move(Locations));
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
