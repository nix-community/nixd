/// \file
/// \brief This implements [Rename].
/// [Rename]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_rename

#include "Convert.h"
#include "Definition.h"

#include "lspserver/Protocol.h"
#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>

#include <exception>

using namespace lspserver;
using namespace nixd;
using namespace llvm;
using namespace nixf;

namespace {

struct RenameException : std::exception {};

struct RenameWithException : RenameException {
  [[nodiscard]] const char *what() const noexcept override {
    return "cannot rename `with` defined variable";
  }
};

struct RenameBuiltinException : RenameException {
  [[nodiscard]] const char *what() const noexcept override {
    return "cannot rename builtin variable";
  }
};

WorkspaceEdit rename(const nixf::Node &Desc, const std::string &NewText,
                     const ParentMapAnalysis &PMA,
                     const VariableLookupAnalysis &VLA, const URIForFile &URI,
                     llvm::StringRef Src) {
  using lspserver::TextEdit;
  // Find "definition"
  auto Def = findDefinition(Desc, PMA, VLA);

  if (Def.source() == Definition::DS_With)
    throw RenameWithException();

  if (Def.isBuiltin())
    throw RenameBuiltinException();

  std::vector<TextEdit> Edits;

  for (const auto *Use : Def.uses()) {
    Edits.emplace_back(TextEdit{
        .range = toLSPRange(Src, Use->range()),
        .newText = NewText,
    });
  }

  Edits.emplace_back(TextEdit{
      .range = toLSPRange(Src, Def.syntax()->range()),
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
        const auto &PM = *TU->parentMap();
        const auto &VLA = *TU->variableLookup();
        try {
          return Reply(rename(*Desc, NewText, PM, VLA, URI, TU->src()));
        } catch (std::exception &E) {
          return Reply(error(E.what()));
        }
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
          return Reply(
              error("cannot find corresponding node on given position"));
        }
        const auto &PM = *TU->parentMap();
        const auto &VLA = *TU->variableLookup();
        try {
          WorkspaceEdit WE = rename(*Desc, "", PM, VLA, URI, TU->src());
          return Reply(toLSPRange(TU->src(), Desc->range()));
        } catch (std::exception &E) {
          return Reply(error(E.what()));
        }
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
