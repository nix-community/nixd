/// \file
/// \brief Implementation of [Go to Definition]
/// [Go to Definition]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_definition

#include "Convert.h"

#include "nixd/Controller/Controller.h"

#include <llvm/Support/Error.h>

#include <boost/asio/post.hpp>

using namespace nixd;
using namespace lspserver;

namespace {

void gotoDefinition(const NixTU &TU, const nixf::Node &AST, nixf::Position Pos,
                    URIForFile URI, Callback<Location> &Reply) {
  using LookupResult = nixf::VariableLookupAnalysis::LookupResult;
  using ResultKind = nixf::VariableLookupAnalysis::LookupResultKind;

  const nixf::Node *N = AST.descend({Pos, Pos});
  if (!N) [[unlikely]] {
    Reply(error("cannot find AST node on given position"));
    return;
  }

  const nixf::ParentMapAnalysis *PMA = TU.parentMap();
  assert(PMA && "ParentMap should not be null as AST is not null");

  const nixf::Node *Var = PMA->upTo(*N, nixf::Node::NK_ExprVar);
  if (!Var) [[unlikely]] {
    Reply(error("cannot find variable on given position"));
    return;
  }
  assert(Var->kind() == nixf::Node::NK_ExprVar);

  // OK, this is an variable. Lookup it in VLA entries.
  const nixf::VariableLookupAnalysis *VLA = TU.variableLookup();
  if (!VLA) [[unlikely]] {
    Reply(error("cannot get variable analysis for nix unit"));
    return;
  }

  LookupResult Result = VLA->query(static_cast<const nixf::ExprVar &>(*Var));
  if (Result.Kind == ResultKind::Undefined) {
    Reply(error("this varaible is undefined"));
    return;
  }

  if (Result.Def->isBuiltin()) {
    Reply(error("this is a builtin variable"));
    return;
  }

  assert(Result.Def->syntax());

  Reply(Location{
      .uri = std::move(URI),
      .range = toLSPRange(Result.Def->syntax()->range()),
  });
}

} // namespace

void Controller::onDefinition(const TextDocumentPositionParams &Params,
                              Callback<Location> Reply) {
  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 Pos = toNixfPosition(Params.position), this]() mutable {
    std::string File(URI.file());
    if (std::shared_ptr<NixTU> TU = getTU(File, Reply)) [[likely]] {
      if (std::shared_ptr<nixf::Node> AST = getAST(*TU, Reply)) [[likely]] {
        gotoDefinition(*TU, *AST, Pos, std::move(URI), Reply);
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
