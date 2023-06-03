#include "nixd/Diagnostic.h"
#include "nixd/EvalDraftStore.h"
#include "nixd/Expr.h"
#include "nixd/Server.h"

#include "lspserver/Connection.h"
#include "lspserver/Logger.h"
#include "lspserver/Path.h"
#include "lspserver/Protocol.h"
#include "lspserver/URI.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/ScopedPrinter.h>
#include <llvm/Support/raw_ostream.h>

#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <utility>
#include <variant>

namespace nixd {

std::tuple<nix::Strings, std::string>
configuration::TopLevel::getInstallable(std::string Fallback) const {
  if (auto Installable = installable) {
    auto ConfigArgs = Installable->args;
    lspserver::log("using client specified installable: [{0}] {1}",
                   llvm::iterator_range(ConfigArgs.begin(), ConfigArgs.end()),
                   Installable->installable);
    return std::tuple{nix::Strings(ConfigArgs.begin(), ConfigArgs.end()),
                      Installable->installable};
  }
  // Fallback to current file otherwise.
  return std::tuple{nix::Strings{"--file", std::move(Fallback)},
                    std::string{""}};
}

void Server::updateWorkspaceVersion() {
  assert(Role == ServerRole::Controller &&
         "Workspace updates must happen in the Controller.");
  WorkspaceVersion++;
  auto To = std::make_unique<nix::Pipe>();
  auto From = std::make_unique<nix::Pipe>();

  To->create();
  From->create();

  auto ForkPID = fork();
  if (ForkPID == -1) {
    lspserver::elog("Cannot create child worker process");
    // TODO reason?
  } else if (ForkPID == 0) {
    // child, it should be a COW fork of the parent process info, as a snapshot
    // we will leave the evaluation task (or any language feature) to the child
    // process, and forward language feature requests to this childs, and choose
    // the best response.
    auto ChildPID = getpid();
    lspserver::elog("created child worker process {0}", ChildPID);

    // Redirect stdin & stdout to our pipes, instead of LSP clients
    dup2(To->readSide.get(), 0);
    dup2(From->writeSide.get(), 1);

    switchToEvaluator();

  } else {

    auto WorkerInputDispatcher =
        std::thread([In = From->readSide.get(), this]() {
          // Start a thread that handles outputs from WorkerProc, and call the
          // Controller Callback
          auto IPort = std::make_unique<lspserver::InboundPort>(In);

          // The loop will exit when the worker process close the pipe.
          IPort->loop(*this);
        });
    WorkerInputDispatcher.detach();

    auto ProcFdStream =
        std::make_unique<llvm::raw_fd_ostream>(To->writeSide.get(), false);

    auto OutPort =
        std::make_unique<lspserver::OutboundPort>(*ProcFdStream, false);

    auto WorkerProc = std::make_unique<Proc>(
        std::move(To), std::move(From), std::move(OutPort),
        std::move(ProcFdStream), ForkPID, WorkspaceVersion,
        std::move(WorkerInputDispatcher));

    Workers.emplace_back(std::move(WorkerProc));
    if (Workers.size() > 5) {
      Workers.pop_front();
    }
  }
}

void Server::addDocument(lspserver::PathRef File, llvm::StringRef Contents,
                         llvm::StringRef Version) {
  using namespace lspserver;

  // Since this file is updated, we first clear its diagnostic
  PublishDiagnosticsParams Notification;
  Notification.uri = URIForFile::canonicalize(File, File);
  Notification.diagnostics = {};
  Notification.version = DraftStore::decodeVersion(Version);
  PublishDiagnostic(Notification);

  DraftMgr.addDraft(File, Version, Contents);
  updateWorkspaceVersion();
}

void Server::onInitialize(const lspserver::InitializeParams &InitializeParams,
                          lspserver::Callback<llvm::json::Value> Reply) {
  ClientCaps = InitializeParams.capabilities;
  llvm::json::Object ServerCaps{
      {"textDocumentSync",
       llvm::json::Object{
           {"openClose", true},
           {"change", (int)lspserver::TextDocumentSyncKind::Incremental},
           {"save", true},
       }},
      {"hoverProvider", true},
      {"completionProvider", llvm::json::Object{{"triggerCharacters", {"."}}}}};

  llvm::json::Object Result{
      {{"serverInfo",
        llvm::json::Object{{"name", "nixd"}, {"version", "0.0.0"}}},
       {"capabilities", std::move(ServerCaps)}}};
  Reply(std::move(Result));
}

void Server::fetchConfig() {
  if (ClientCaps.WorkspaceConfiguration) {
    WorkspaceConfiguration(
        lspserver::ConfigurationParams{
            std::vector<lspserver::ConfigurationItem>{
                lspserver::ConfigurationItem{.section = "nixd"}}},
        [this](llvm::Expected<configuration::TopLevel> Response) {
          if (Response) {
            Config = std::move(Response.get());
          }
        });
  }
}

std::string Server::encodeVersion(std::optional<int64_t> LSPVersion) {
  return LSPVersion ? llvm::to_string(*LSPVersion) : "";
}

std::shared_ptr<const std::string>
Server::getDraft(lspserver::PathRef File) const {
  auto Draft = DraftMgr.getDraft(File);
  if (!Draft)
    return nullptr;
  return std::move(Draft->Contents);
}

Server::Server(std::unique_ptr<lspserver::InboundPort> In,
               std::unique_ptr<lspserver::OutboundPort> Out, int WaitWorker)
    : LSPServer(std::move(In), std::move(Out)), WaitWorker(WaitWorker) {

  Registry.addMethod("initialize", this, &Server::onInitialize);
  Registry.addMethod("textDocument/hover", this, &Server::onHover);
  Registry.addMethod("textDocument/completion", this, &Server::onCompletion);
  Registry.addNotification("initialized", this, &Server::onInitialized);

  // Text Document Synchronization
  Registry.addNotification("textDocument/didOpen", this,
                           &Server::onDocumentDidOpen);
  Registry.addNotification("textDocument/didChange", this,
                           &Server::onDocumentDidChange);

  // Workspace
  Registry.addNotification("workspace/didChangeConfiguration", this,
                           &Server::onWorkspaceDidChangeConfiguration);
  PublishDiagnostic = mkOutNotifiction<lspserver::PublishDiagnosticsParams>(
      "textDocument/publishDiagnostics");
  WorkspaceConfiguration =
      mkOutMethod<lspserver::ConfigurationParams, configuration::TopLevel>(
          "workspace/configuration");

  /// IPC
  Registry.addNotification("nixd/ipc/diagnostic", this,
                           &Server::onWorkerDiagnostic);

  Registry.addMethod("nixd/ipc/textDocument/completion", this,
                     &Server::onWorkerCompletion);
}

void Server::onDocumentDidOpen(
    const lspserver::DidOpenTextDocumentParams &Params) {
  lspserver::PathRef File = Params.textDocument.uri.file();

  const std::string &Contents = Params.textDocument.text;

  addDocument(File, Contents, encodeVersion(Params.textDocument.version));
}

void Server::onDocumentDidChange(
    const lspserver::DidChangeTextDocumentParams &Params) {
  lspserver::PathRef File = Params.textDocument.uri.file();
  auto Code = getDraft(File);
  if (!Code) {
    lspserver::log("Trying to incrementally change non-added document: {0}",
                   File);
    return;
  }
  std::string NewCode(*Code);
  for (const auto &Change : Params.contentChanges) {
    if (auto Err = applyChange(NewCode, Change)) {
      // If this fails, we are most likely going to be not in sync anymore
      // with the client.  It is better to remove the draft and let further
      // operations fail rather than giving wrong results.
      removeDocument(File);
      lspserver::elog("Failed to update {0}: {1}", File, std::move(Err));
      return;
    }
  }
  addDocument(File, NewCode, encodeVersion(Params.textDocument.version));
}

//-----------------------------------------------------------------------------/
// Diagnostics

void Server::clearDiagnostic(lspserver::PathRef Path) {
  lspserver::URIForFile Uri = lspserver::URIForFile::canonicalize(Path, Path);
  clearDiagnostic(Uri);
}

void Server::clearDiagnostic(const lspserver::URIForFile &FileUri) {
  lspserver::PublishDiagnosticsParams Notification;
  Notification.uri = FileUri;
  Notification.diagnostics = {};
  PublishDiagnostic(Notification);
}

void Server::onWorkerDiagnostic(const ipc::Diagnostics &Diag) {
  lspserver::log("received diagnostic from worker: {0}", Diag.WorkspaceVersion);

  {
    std::lock_guard<std::mutex> Guard(DiagStatusLock);
    if (DiagStatus.WorkspaceVersion > Diag.WorkspaceVersion) {
      // Skip this diagnostic, because it is outdated.
      return;
    }

    // Update client diagnostics
    DiagStatus.WorkspaceVersion = Diag.WorkspaceVersion;

    for (const auto &PublishedDiags : DiagStatus.ClientDiags)
      clearDiagnostic(PublishedDiags.uri);

    DiagStatus.ClientDiags = Diag.Params;

    for (const auto &Diag : Diag.Params) {
      PublishDiagnostic(Diag);
    }
  }
}

//-----------------------------------------------------------------------------/
// Completion

void Server::onCompletion(
    const lspserver::CompletionParams &Params,
    lspserver::Callback<lspserver::CompletionList> Reply) {
  auto Thread = std::thread([=, Reply = std::move(Reply), this]() mutable {
    // For all active workers, send the completion request
    auto ListStore = std::make_shared<std::vector<lspserver::CompletionList>>(
        Workers.size());
    auto ListStoreLock = std::make_shared<std::mutex>();

    size_t I = 0;
    for (const auto &Worker : Workers) {
      auto ComplectionRequest =
          mkOutMethod<lspserver::CompletionParams, lspserver::CompletionList>(
              "nixd/ipc/textDocument/completion", Worker->OutPort.get());

      ComplectionRequest(
          Params, [I, ListStore, ListStoreLock](
                      llvm::Expected<lspserver::CompletionList> Result) {
            // The worker answered our request, fill the completion
            // lists then.
            if (Result) {
              lspserver::log(
                  "received result from our client, which has {0} item(s)",
                  Result.get().items.size());
              {
                std::lock_guard Guard(*ListStoreLock);
                (*ListStore)[I] = Result.get();
              }
            }
          });
      I++;
    }
    // Wait for our client, this is currently hardcoded
    usleep(5e5);

    // Reset the store ptr in event handling module, so that client reply after
    // 'usleep' will not write the store (to avoid data race)
    std::lock_guard Guard(*ListStoreLock);

    // brute-force iterating over the result, and choose the biggest item
    size_t BestIdx = 0;
    size_t BestSize = 0;
    for (size_t I = 0; I < ListStore->size(); I++) {
      auto LSize = ListStore->at(I).items.size();
      if (LSize >= BestSize) {
        BestIdx = I;
        BestSize = LSize;
      }
    }
    // And finally, reply our client
    lspserver::log("chosed {0} completion lists.", BestSize);
    Reply(ListStore->at(BestIdx));
  });

  Thread.detach();
}

void Server::onHover(const lspserver::TextDocumentPositionParams &,
                     lspserver::Callback<llvm::json::Value>) {}

} // namespace nixd
