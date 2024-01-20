/// \file
/// \brief Controller. The process interacting with users.
#include "nixd-config.h"

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Parse/Parser.h"

#include "lspserver/DraftStore.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Path.h"
#include "lspserver/Protocol.h"
#include "lspserver/SourceCode.h"

#include <llvm/Support/JSON.h>

#include <sstream>

namespace {

using namespace lspserver;
using namespace llvm::json;

Position toLSPPosition(const nixf::Point &P) {
  return Position{static_cast<int>(P.line()), static_cast<int>(P.column())};
}

Range toLSPRange(const nixf::RangeTy &R) {
  return Range{toLSPPosition(R.begin()), toLSPPosition(R.end())};
}

int getLSPSeverity(nixf::Diagnostic::DiagnosticKind Kind) {
  switch (nixf::Diagnostic::severity(Kind)) {
  case nixf::Diagnostic::DS_Fatal:
  case nixf::Diagnostic::DS_Error:
    return 1;
  case nixf::Diagnostic::DS_Warning:
    return 2;
  }
  assert(false && "Invalid severity");
}

class Controller : public LSPServer {
  DraftStore Store;

  llvm::unique_function<void(const lspserver::PublishDiagnosticsParams &)>
      PublishDiagnostic;

  /// Action right after a document is added (including updates).
  void actOnDocumentAdd(PathRef File, std::optional<int64_t> Version) {
    auto Draft = Store.getDraft(File);
    assert(Draft && "Added document is not in the store?");
    std::vector<nixf::Diagnostic> Diagnostics;
    nixf::parse(*Draft->Contents, Diagnostics);

    std::vector<Diagnostic> LSPDiags;
    LSPDiags.reserve(Diagnostics.size());
    for (const nixf::Diagnostic &D : Diagnostics) {
      LSPDiags.emplace_back(Diagnostic{
          .range = toLSPRange(D.range()),
          .severity = getLSPSeverity(D.kind()),
          .code = D.sname(),
          .source = "nixf",
          .message = D.format(),
      });
      for (const nixf::Note &N : D.notes()) {
        LSPDiags.emplace_back(Diagnostic{
            .range = toLSPRange(N.range()),
            .severity = 3,
            .source = "nixf",
            .message = N.format(),
        });
      }
    }
    PublishDiagnostic({
        .uri = URIForFile::canonicalize(File, File),
        .diagnostics = std::move(LSPDiags),
        .version = Version,
    });
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

    PublishDiagnostic = mkOutNotifiction<PublishDiagnosticsParams>(
        "textDocument/publishDiagnostics");
  }
  void onInitialized([[maybe_unused]] const InitializedParams &Params) {}

  void onDocumentDidOpen(const DidOpenTextDocumentParams &Params) {
    PathRef File = Params.textDocument.uri.file();
    const std::string &Contents = Params.textDocument.text;
    std::optional<int64_t> Version = Params.textDocument.version;
    Store.addDraft(File, DraftStore::encodeVersion(Version), Contents);
    actOnDocumentAdd(File, Version);
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
    std::optional<int64_t> Version = Params.textDocument.version;
    Store.addDraft(File, DraftStore::encodeVersion(Version), NewCode);
    actOnDocumentAdd(File, Version);
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
