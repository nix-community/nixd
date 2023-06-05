#include "nixd/Diagnostic.h"
#include "nixd/EvalDraftStore.h"
#include "nixd/Expr.h"
#include "nixd/Server.h"
#include "nixd/Support.h"

#include "lspserver/Connection.h"
#include "lspserver/Logger.h"
#include "lspserver/Path.h"
#include "lspserver/Protocol.h"
#include "lspserver/URI.h"

#include <llvm/ADT/FunctionExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/ScopedPrinter.h>
#include <llvm/Support/raw_ostream.h>

#include <boost/iostreams/stream.hpp>
#include <boost/process.hpp>

#include <cstdint>
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

void Server::updateWorkspaceVersion(lspserver::PathRef File) {
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

    switchToEvaluator(File);

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

    auto WorkerProc = std::unique_ptr<Proc>(
        new Proc{.ToPipe = std::move(To),
                 .FromPipe = std::move(From),
                 .OutPort = std::move(OutPort),
                 .OwnedStream = std::move(ProcFdStream),
                 .Pid = ForkPID,
                 .WorkspaceVersion = WorkspaceVersion,
                 .InputDispatcher = std::move(WorkerInputDispatcher)});

    Workers.emplace_back(std::move(WorkerProc));
    if (Workers.size() > Config.getNumWorkers()) {
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
  updateWorkspaceVersion(File);
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
      {"definitionProvider", true},
      {"hoverProvider", true},
      {"documentFormattingProvider", true},
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
  Registry.addMethod("textDocument/definition", this, &Server::onDefinition);
  Registry.addMethod("textDocument/formatting", this, &Server::onFormat);

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

  Registry.addMethod("nixd/ipc/textDocument/hover", this,
                     &Server::onWorkerHover);

  Registry.addMethod("nixd/ipc/textDocument/definition", this,
                     &Server::onWorkerDefinition);
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
// Location Search: goto definition, declaration, ...

void Server::onDefinition(const lspserver::TextDocumentPositionParams &Params,
                          lspserver::Callback<llvm::json::Value> Reply) {
  auto Thread = std::thread([=, Reply = std::move(Reply), this]() mutable {
    auto Responses =
        askWorkers<lspserver::TextDocumentPositionParams, lspserver::Location>(
            Workers, "nixd/ipc/textDocument/definition", Params, 2e4);

    Reply(latestMatchOr<lspserver::Location, llvm::json::Value>(
        Responses,
        [](const lspserver::Location &Location) -> bool {
          return Location.range.start.line != -1;
        },
        llvm::json::Object{}));
  });

  Thread.detach();
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
    auto Responses =
        askWorkers<lspserver::CompletionParams, lspserver::CompletionList>(
            Workers, "nixd/ipc/textDocument/completion", Params, 5e4);
    Reply(bestMatchOr<lspserver::CompletionList>(
        Responses, [](const lspserver::CompletionList &L) -> uint64_t {
          return L.items.size() + 1;
        }));
  });
  Thread.detach();
}

void Server::onHover(const lspserver::TextDocumentPositionParams &Params,
                     lspserver::Callback<lspserver::Hover> Reply) {
  auto Thread = std::thread([=, Reply = std::move(Reply), this]() mutable {
    auto Responses =
        askWorkers<lspserver::TextDocumentPositionParams, lspserver::Hover>(
            Workers, "nixd/ipc/textDocument/hover", Params, 2e4);
    Reply(latestMatchOr<lspserver::Hover>(
        Responses, [](const lspserver::Hover &H) {
          return H.contents.value.length() != 0;
        }));
  });

  Thread.detach();
}

void Server::onFormat(
    const lspserver::DocumentFormattingParams &Params,
    lspserver::Callback<std::vector<lspserver::TextEdit>> Reply) {

  auto Thread = std::thread([=, Reply = std::move(Reply), this]() mutable {
    lspserver::PathRef File = Params.textDocument.uri.file();
    auto Code = *getDraft(File);
    std::string FormattedCode;

    namespace bp = boost::process;

    bp::opstream To;
    bp::ipstream From;

    bp::child Fmt("nixpkgs-fmt", bp::std_out > From, bp::std_in < To);

    To << Code;
    To.flush();

    To.pipe().close();

    while (From.good() && !From.eof()) {
      std::string Buf;
      std::getline(From, Buf, {});
      FormattedCode += Buf;
    }
    Fmt.wait();

    lspserver::TextEdit E{{{0, 0}, {INT_MAX, INT_MAX}}, FormattedCode};

    Reply(std::vector{E});
  });
  Thread.detach();
}
} // namespace nixd
