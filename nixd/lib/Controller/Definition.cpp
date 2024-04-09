/// \file
/// \brief Implementation of [Go to Definition]
/// [Go to Definition]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_definition

#include "Definition.h"
#include "Convert.h"

#include "nixd/Controller/Controller.h"
#include "nixf/Sema/VariableLookup.h"

#include <llvm/Support/Error.h>

#include <boost/asio/post.hpp>

using namespace nixd;
using namespace nixf;
using namespace lspserver;
using namespace llvm;

using LookupResult = VariableLookupAnalysis::LookupResult;
using ResultKind = VariableLookupAnalysis::LookupResultKind;

namespace {

void gotoDefinition(const NixTU &TU, const Node &AST, nixf::Position Pos,
                    URIForFile URI, Callback<Location> &Reply) {
  const Node *N = AST.descend({Pos, Pos});
  if (!N) [[unlikely]] {
    Reply(error("cannot find AST node on given position"));
    return;
  }

  const ParentMapAnalysis *PMA = TU.parentMap();
  assert(PMA && "ParentMap should not be null as AST is not null");

  // OK, this is an variable. Lookup it in VLA entries.
  const VariableLookupAnalysis *VLA = TU.variableLookup();
  if (!VLA) [[unlikely]] {
    Reply(error("cannot get variable analysis for nix unit"));
    return;
  }

  if (Expected<const Definition &> ExpDef = findDefinition(*N, *PMA, *VLA)) {
    assert(ExpDef->syntax());
    Reply(Location{
        .uri = std::move(URI),
        .range = toLSPRange(ExpDef->syntax()->range()),
    });
  } else {
    Reply(ExpDef.takeError());
  }
}

const Definition *findSelfDefinition(const Node &N,
                                     const ParentMapAnalysis &PMA,
                                     const VariableLookupAnalysis &VLA) {
  // If "N" is a definition itself, just return it.
  if (const Definition *Def = VLA.toDef(N))
    return Def;

  // If N is inside an attrset, it maybe an "AttrName", let's look for it.
  const Node *Parent = PMA.query(N);
  if (Parent && Parent->kind() == Node::NK_AttrName)
    return VLA.toDef(*Parent);

  return nullptr;
}

} // namespace

Expected<const Definition &>
nixd::findDefinition(const Node &N, const ParentMapAnalysis &PMA,
                     const VariableLookupAnalysis &VLA) {
  const Node *Var = PMA.upTo(N, Node::NK_ExprVar);
  if (!Var) [[unlikely]] {
    if (const Definition *Def = findSelfDefinition(N, PMA, VLA))
      return *Def;
    return error("cannot find variable on given position");
  }
  assert(Var->kind() == Node::NK_ExprVar);
  LookupResult Result = VLA.query(static_cast<const ExprVar &>(*Var));

  assert(Result.Def);

  if (Result.Kind == ResultKind::Undefined)
    return error("this varaible is undefined");

  if (Result.Def->isBuiltin())
    return error("this is a builtin variable");

  return *Result.Def;
}

void Controller::onDefinition(const TextDocumentPositionParams &Params,
                              Callback<Location> Reply) {
  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 Pos = toNixfPosition(Params.position), this]() mutable {
    std::string File(URI.file());
    if (std::shared_ptr<NixTU> TU = getTU(File, Reply)) [[likely]] {
      if (std::shared_ptr<Node> AST = getAST(*TU, Reply)) [[likely]] {
        gotoDefinition(*TU, *AST, Pos, std::move(URI), Reply);
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
