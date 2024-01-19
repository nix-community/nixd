/// \file
/// \brief Controller. The process interacting with users.
#include "nixd-config.h"

#include "lspserver/DraftStore.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Path.h"
#include "lspserver/SourceCode.h"

#include <llvm/Support/JSON.h>

namespace {

using namespace lspserver;
using namespace llvm::json;

class Controller : public LSPServer {
  DraftStore Store;

  void addDocument(PathRef File, llvm::StringRef Contents,
                   llvm::StringRef Version) {
    Store.addDraft(File, Version, Contents);
  }

  void removeDocument(PathRef File) { Store.removeDraft(File); }

  void onInitialize( // NOLINT(readability-convert-member-functions-to-static)
      [[maybe_unused]] const InitializeParams &Params, Callback<Value> Reply) {

    Object ServerCaps{
        {
            {"textDocumentSync",
             llvm::json::Object{
                 {"openClose", true},
                 {"change", (int)TextDocumentSyncKind::Incremental},
                 {"save", true},
             }},
        },
    };

    Object Result{{
        {"serverInfo",
         Object{
             {"name", "nixd"},
             {"version", NIXD_VERSION},
         }},
        {"capabilities", std::move(ServerCaps)},
    }};

    Reply(std::move(Result));
  }
  void onInitialized([[maybe_unused]] const InitializedParams &Params) {}

  void onDocumentDidOpen(const DidOpenTextDocumentParams &Params) {
    PathRef File = Params.textDocument.uri.file();
    const std::string &Contents = Params.textDocument.text;
    addDocument(File, Contents,
                DraftStore::encodeVersion(Params.textDocument.version));
  }

  void onDocumentDidChange(const DidChangeTextDocumentParams &Params) {
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
    std::string Version =
        DraftStore::encodeVersion(Params.textDocument.version);
    addDocument(File, NewCode, Version);
  }

  void onDocumentDidClose(const DidCloseTextDocumentParams &Params) {
    PathRef File = Params.textDocument.uri.file();
    removeDocument(File);
  }

public:
  Controller(std::unique_ptr<InboundPort> In, std::unique_ptr<OutboundPort> Out)
      : LSPServer(std::move(In), std::move(Out)) {

    // Life Cycle
    Registry.addMethod("initialize", this, &Controller::onInitialize);
    Registry.addNotification("initialized", this, &Controller::onInitialized);

    // Text Document Synchronization
    Registry.addNotification("textDocument/didOpen", this,
                             &Controller::onDocumentDidOpen);
    Registry.addNotification("textDocument/didChange", this,
                             &Controller::onDocumentDidChange);

    Registry.addNotification("textDocument/didClose", this,
                             &Controller::onDocumentDidClose);
  }
};

} // namespace

namespace nixd {
void runController(std::unique_ptr<InboundPort> In,
                   std::unique_ptr<OutboundPort> Out) {
  Controller C(std::move(In), std::move(Out));
  C.run();
}
} // namespace nixd
