/// \file
/// \brief This implements [Rename].
/// [Rename]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_rename

#include "Convert.h"
#include "Definition.h"

#include "lspserver/Protocol.h"
#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>

using namespace lspserver;
using namespace nixd;
using namespace llvm;
using namespace nixf;

namespace {
llvm::Expected<WorkspaceEdit> rename(const nixf::Node &Desc,
                                     const std::string &NewText,
                                     const ParentMapAnalysis &PMA,
                                     const VariableLookupAnalysis &VLA,
                                     const URIForFile &URI) {
  using lspserver::TextEdit;
  // Find "definition"
  auto Def = findDefinition(Desc, PMA, VLA);
  if (!Def)
    return Def.takeError();

  std::vector<TextEdit> Edits;

  for (const auto *Use : Def->uses()) {
    Edits.emplace_back(TextEdit{
        .range = toLSPRange(Use->range()),
        .newText = NewText,
    });
  }

  Edits.emplace_back(TextEdit{
      .range = toLSPRange(Def->syntax()->range()),
      .newText = NewText,
  });
  WorkspaceEdit WE;
  WE.changes = std::map<std::string, std::vector<TextEdit>>{
      {URI.uri(), std::move(Edits)}};
  return WE;
}
} // namespace

void Controller::onRename(const RenameParams &Params,
                          Callback<WorkspaceEdit> Reply) {
  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 Pos = toNixfPosition(Params.position),
                 NewText = Params.newName, this]() mutable {
    std::string File(URI.file());
    if (std::shared_ptr<NixTU> TU = getTU(File, Reply)) [[likely]] {
      if (std::shared_ptr<nixf::Node> AST = getAST(*TU, Reply)) [[likely]] {
        const nixf::Node *Desc = AST->descend({Pos, Pos});
        if (!Desc) {
          Reply(error("cannot find corresponding node on given position"));
          return;
        }
        Reply(rename(*Desc, NewText, *TU->parentMap(), *TU->variableLookup(),
                     URI));
        return;
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}

void Controller::onPrepareRename(
    const lspserver::TextDocumentPositionParams &Params,
    Callback<lspserver::Range> Reply) {
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
        llvm::Expected<WorkspaceEdit> Exp =
            rename(*Desc, "", *TU->parentMap(), *TU->variableLookup(), URI);
        if (Exp) {
          Reply(toLSPRange(Desc->range()));
          return;
        }
        Reply(Exp.takeError());
        return;
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
