/// \file
/// \brief Implementation of the [text document
/// sync](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_synchronization).
#include "Controller.h"

#include "lspserver/SourceCode.h"

namespace nixd {

using namespace llvm::json;
using namespace lspserver;

void Controller::onDocumentDidOpen(
    const lspserver::DidOpenTextDocumentParams &Params) {
  PathRef File = Params.textDocument.uri.file();
  const std::string &Contents = Params.textDocument.text;
  std::optional<int64_t> Version = Params.textDocument.version;
  Store.addDraft(File, DraftStore::encodeVersion(Version), Contents);
  actOnDocumentAdd(File, Version);
}

void Controller::onDocumentDidChange(
    const DidChangeTextDocumentParams &Params) {
  PathRef File = Params.textDocument.uri.file();
  auto Code = Store.getDraft(File);
  if (!Code) {
    log("Trying to incrementally change non-added document: {0}", File);
    return;
  }
  std::string NewCode(*Code->Contents);
  for (const auto &Change : Params.contentChanges) {
    if (auto Err = applyChange(NewCode, Change)) {
      // If this fails, we are most likely going to be not in sync anymore
      // with the client.  It is better to remove the draft and let further
      // operations fail rather than giving wrong results.
      removeDocument(File);
      elog("Failed to update {0}: {1}", File, std::move(Err));
      return;
    }
  }
  std::optional<int64_t> Version = Params.textDocument.version;
  Store.addDraft(File, DraftStore::encodeVersion(Version), NewCode);
  actOnDocumentAdd(File, Version);
}

void Controller::onDocumentDidClose(const DidCloseTextDocumentParams &Params) {
  PathRef File = Params.textDocument.uri.file();
  removeDocument(File);
}

} // namespace nixd
