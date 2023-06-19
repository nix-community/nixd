#include "nixd-config.h"

#include "nixd/Diagnostic.h"
#include "nixd/EvalDraftStore.h"
#include "nixd/Expr.h"
#include "nixd/Server.h"
#include "nixd/Support.h"

#include "lspserver/Connection.h"
#include "lspserver/Logger.h"
#include "lspserver/Path.h"
#include "lspserver/Protocol.h"
#include "lspserver/SourceCode.h"
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
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include <unistd.h>

namespace nixd {

void Server::forkWorker(llvm::unique_function<void()> WorkerAction,
                        std::deque<std::unique_ptr<Proc>> &WorkerPool) {
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

    WorkerAction();

  } else {

    auto WorkerInputDispatcher =
        std::thread([In = From->readSide.get(), this]() {
          // Start a thread that handles outputs from WorkerProc, and call the
          // Controller Callback
          auto IPort = std::make_unique<lspserver::InboundPort>(In);

          // The loop will exit when the worker process close the pipe.
          IPort->loop(*this);
        });

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
                 .InputDispatcher = std::move(WorkerInputDispatcher),
                 .EvalDoneSmp = std::ref(EvalDoneSmp),
                 .WaitWorker = WaitWorker});

    WorkerPool.emplace_back(std::move(WorkerProc));
  }
}

void Server::updateWorkspaceVersion(lspserver::PathRef File) {
  assert(Role == ServerRole::Controller &&
         "Workspace updates must happen in the Controller.");
  WorkspaceVersion++;
  std::lock_guard Guard(EvalWorkerLock);
  forkWorker(
      [File, this]() {
        switchToEvaluator(File);
        // unregister "Procs" in worker process, they are managed by controller
        for (auto &Worker : EvalWorkers)
          Worker->Pid.release();
      },
      EvalWorkers);
  if (EvalWorkers.size() >
          static_cast<decltype(EvalWorkers.size())>(Config.getNumWorkers()) &&
      !WaitWorker)
    EvalWorkers.pop_front();
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

void Server::updateConfig(configuration::TopLevel &&NewConfig) {
  Config = std::move(NewConfig);
  forkOptionWorker();
}

void Server::fetchConfig() {
  if (ClientCaps.WorkspaceConfiguration) {
    WorkspaceConfiguration(
        lspserver::ConfigurationParams{
            std::vector<lspserver::ConfigurationItem>{
                lspserver::ConfigurationItem{.section = "nixd"}}},
        [this](llvm::Expected<configuration::TopLevel> Response) {
          if (Response) {
            updateConfig(std::move(Response.get()));
          }
        });
  }
}

llvm::Expected<configuration::TopLevel>
Server::parseConfig(llvm::StringRef JSON) {
  using namespace configuration;

  auto ExpectedValue = llvm::json::parse(JSON);
  if (!ExpectedValue)
    return ExpectedValue.takeError();
  TopLevel NewConfig;
  llvm::json::Path::Root P;
  if (fromJSON(ExpectedValue.get(), NewConfig, P))
    return NewConfig;
  return lspserver::error("value cannot be converted to internal config type");
}

void Server::readJSONConfig(lspserver::PathRef File) noexcept {
  try {
    std::string ConfigStr;
    std::ostringstream SS;
    std::ifstream In(File.str(), std::ios::in);
    SS << In.rdbuf();

    if (auto NewConfig = parseConfig(SS.str()))
      updateConfig(std::move(NewConfig.get()));
    else {
      throw nix::Error("configuration cannot be parsed");
    }
  } catch (std::exception &E) {
  } catch (...) {
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
  // Life Cycle
  Registry.addMethod("initialize", this, &Server::onInitialize);
  Registry.addNotification("initialized", this, &Server::onInitialized);

  // Text Document Synchronization
  Registry.addNotification("textDocument/didOpen", this,
                           &Server::onDocumentDidOpen);
  Registry.addNotification("textDocument/didChange", this,
                           &Server::onDocumentDidChange);

  Registry.addNotification("textDocument/didClose", this,
                           &Server::onDocumentDidClose);

  // Language Features
  Registry.addMethod("textDocument/documentLink", this,
                     &Server::onDocumentLink);
  Registry.addMethod("textDocument/documentSymbol", this,
                     &Server::onDocumentSymbol);
  Registry.addMethod("textDocument/hover", this, &Server::onHover);
  Registry.addMethod("textDocument/completion", this, &Server::onCompletion);
  Registry.addMethod("textDocument/declaration", this, &Server::onDecalration);
  Registry.addMethod("textDocument/definition", this, &Server::onDefinition);
  Registry.addMethod("textDocument/formatting", this, &Server::onFormat);
  Registry.addMethod("textDocument/rename", this, &Server::onRename);
  Registry.addMethod("textDocument/prepareRename", this,
                     &Server::onPrepareRename);

  PublishDiagnostic = mkOutNotifiction<lspserver::PublishDiagnosticsParams>(
      "textDocument/publishDiagnostics");

  // Workspace
  Registry.addNotification("workspace/didChangeConfiguration", this,
                           &Server::onWorkspaceDidChangeConfiguration);
  WorkspaceConfiguration =
      mkOutMethod<lspserver::ConfigurationParams, configuration::TopLevel>(
          "workspace/configuration");

  /// IPC
  Registry.addNotification("nixd/ipc/diagnostic", this,
                           &Server::onEvalDiagnostic);

  Registry.addMethod("nixd/ipc/textDocument/completion", this,
                     &Server::onEvalCompletion);

  Registry.addMethod("nixd/ipc/textDocument/documentLink", this,
                     &Server::onEvalDocumentLink);
  Registry.addMethod("nixd/ipc/textDocument/documentSymbol", this,
                     &Server::onEvalDocumentSymbol);

  Registry.addMethod("nixd/ipc/textDocument/hover", this, &Server::onEvalHover);

  Registry.addMethod("nixd/ipc/textDocument/definition", this,
                     &Server::onEvalDefinition);

  Registry.addMethod("nixd/ipc/textDocument/rename", this,
                     &Server::onEvalRename);

  Registry.addMethod("nixd/ipc/option/textDocument/declaration", this,
                     &Server::onOptionDeclaration);

  Registry.addNotification("nixd/ipc/eval/finished", this,
                           &Server::onEvalFinished);

  readJSONConfig();
}

//-----------------------------------------------------------------------------/
// Life Cycle

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
      {"declarationProvider", true},
      {"definitionProvider", true},
      {"documentLinkProvider", true},
      {"documentSymbolProvider", true},
      {"hoverProvider", true},
      {"documentFormattingProvider", true},
      {
          "completionProvider",
          llvm::json::Object{{"triggerCharacters", {"."}}},
      },
      {"renameProvider", llvm::json::Object{{"prepareProvider", true}}}};

  llvm::json::Object Result{
      {{"serverInfo",
        llvm::json::Object{{"name", "nixd"}, {"version", NIXD_VERSION}}},
       {"capabilities", std::move(ServerCaps)}}};
  Reply(std::move(Result));
}

//-----------------------------------------------------------------------------/
// Text Document Synchronization

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

void Server::onDocumentDidClose(
    const lspserver::DidCloseTextDocumentParams &Params) {
  lspserver::PathRef File = Params.textDocument.uri.file();
  auto Code = getDraft(File);
  removeDocument(File);
}

//-----------------------------------------------------------------------------/
// Language Features

void Server::onDecalration(const lspserver::TextDocumentPositionParams &Params,
                           lspserver::Callback<llvm::json::Value> Reply) {
  if (!Config.options || !Config.options->enable.value_or(false)) {
    Reply(nullptr);
    return;
  }

  auto Task = [=, Reply = std::move(Reply), this]() mutable {
    ReplyRAII<llvm::json::Value> RR(std::move(Reply));

    // Set the default response to "null", instead of errors
    RR.Response = nullptr;

    ipc::AttrPathParams APParams;

    // Try to get current attribute path, expand the position

    auto Code = llvm::StringRef(
        *DraftMgr.getDraft(Params.textDocument.uri.file())->Contents);
    auto ExpectedOffset = positionToOffset(Code, Params.position);

    if (!ExpectedOffset)
      RR.Response = ExpectedOffset.takeError();

    auto Offset = ExpectedOffset.get();
    auto From = Offset;
    auto To = Offset;

    std::string Punc = "\r\n\t ;";

    auto IsPunc = [&Punc](char C) {
      return std::ranges::any_of(Punc, [C](char Ck) { return Ck == C; });
    };

    for (; From >= 0 && !IsPunc(Code[From]);)
      From--;
    for (; To < Code.size() && !IsPunc(Code[To]);)
      To++;

    APParams.Path = Code.substr(From, To - From).trim(Punc);
    lspserver::log("requesting path: {0}", APParams.Path);

    auto Responses = askWorkers<ipc::AttrPathParams, lspserver::Location>(
        OptionWorkers, OptionWorkerLock,
        "nixd/ipc/option/textDocument/declaration", APParams, 2e4);

    if (Responses.empty())
      return;

    RR.Response = std::move(Responses.back());
  };

  boost::asio::post(Pool, std::move(Task));
}

void Server::onDefinition(const lspserver::TextDocumentPositionParams &Params,
                          lspserver::Callback<llvm::json::Value> Reply) {
  auto Task = [=, Reply = std::move(Reply), this]() mutable {
    auto Responses =
        askWorkers<lspserver::TextDocumentPositionParams, lspserver::Location>(
            EvalWorkers, EvalWorkerLock, "nixd/ipc/textDocument/definition",
            Params, 2e4);

    Reply(latestMatchOr<lspserver::Location, llvm::json::Value>(
        Responses,
        [](const lspserver::Location &Location) -> bool {
          return Location.range.start.line != -1;
        },
        llvm::json::Object{}));
  };

  boost::asio::post(Pool, std::move(Task));
}

void Server::onDocumentLink(
    const lspserver::DocumentLinkParams &Params,
    lspserver::Callback<std::vector<lspserver::DocumentLink>> Reply) {
  auto Task = [=, Reply = std::move(Reply), this]() mutable {
    auto Responses = askWorkers<lspserver::TextDocumentIdentifier,
                                std::vector<lspserver::DocumentLink>>(
        EvalWorkers, EvalWorkerLock, "nixd/ipc/textDocument/documentLink",
        Params.textDocument, 2e4);

    Reply(latestMatchOr<std::vector<lspserver::DocumentLink>>(
        Responses, [](const std::vector<lspserver::DocumentLink> &) -> bool {
          return true;
        }));
  };

  boost::asio::post(Pool, std::move(Task));
}

void Server::onDocumentSymbol(
    const lspserver::DocumentSymbolParams &Params,
    lspserver::Callback<std::vector<lspserver::DocumentSymbol>> Reply) {

  auto Task = [=, Reply = std::move(Reply), this]() mutable {
    auto Responses = askWorkers<lspserver::TextDocumentIdentifier,
                                std::vector<lspserver::DocumentSymbol>>(
        EvalWorkers, EvalWorkerLock, "nixd/ipc/textDocument/documentSymbol",
        Params.textDocument, 1e6);

    Reply(latestMatchOr<std::vector<lspserver::DocumentSymbol>>(
        Responses, [](const std::vector<lspserver::DocumentSymbol> &) -> bool {
          return true;
        }));
  };

  boost::asio::post(Pool, std::move(Task));
}

void Server::onHover(const lspserver::TextDocumentPositionParams &Params,
                     lspserver::Callback<lspserver::Hover> Reply) {
  auto Task = [=, Reply = std::move(Reply), this]() mutable {
    auto Responses =
        askWorkers<lspserver::TextDocumentPositionParams, lspserver::Hover>(
            EvalWorkers, EvalWorkerLock, "nixd/ipc/textDocument/hover", Params,
            2e4);
    Reply(latestMatchOr<lspserver::Hover>(
        Responses, [](const lspserver::Hover &H) {
          return H.contents.value.length() != 0;
        }));
  };

  boost::asio::post(Pool, std::move(Task));
}

void Server::onCompletion(
    const lspserver::CompletionParams &Params,
    lspserver::Callback<lspserver::CompletionList> Reply) {
  auto EnableOption =
      bool(Config.options) && Config.options->enable.value_or(false);
  auto Task = [=, Reply = std::move(Reply), this]() mutable {
    auto Responses =
        askWorkers<lspserver::CompletionParams, lspserver::CompletionList>(
            EvalWorkers, EvalWorkerLock, "nixd/ipc/textDocument/completion",
            Params, 1e6);

    if (EnableOption) {
      ipc::AttrPathParams APParams;

      if (Params.context.triggerCharacter == ".") {
        // Get nixpkgs options
        auto Code = llvm::StringRef(
            *DraftMgr.getDraft(Params.textDocument.uri.file())->Contents);
        auto ExpectedPosition = positionToOffset(Code, Params.position);

        // get the attr path
        auto TruncateBackCode = Code.substr(0, ExpectedPosition.get());

        auto [_, AttrPath] = TruncateBackCode.rsplit(" ");
        APParams.Path = AttrPath.str();
      }

      auto RespOption =
          askWorkers<ipc::AttrPathParams, lspserver::CompletionList>(
              OptionWorkers, OptionWorkerLock,
              "nixd/ipc/textDocument/completion/options", APParams, 5e4);

      Responses.insert(Responses.end(), RespOption.begin(), RespOption.end());
    }
    Reply(latestMatchOr<lspserver::CompletionList>(
        Responses,
        [](const lspserver::CompletionList &L) -> bool { return true; }));
  };
  boost::asio::post(Pool, std::move(Task));
}

void Server::onRename(const lspserver::RenameParams &Params,
                      lspserver::Callback<lspserver::WorkspaceEdit> Reply) {

  auto Task = [=, Reply = std::move(Reply), this]() mutable {
    auto Responses =
        askWorkers<lspserver::RenameParams, std::vector<lspserver::TextEdit>>(
            EvalWorkers, EvalWorkerLock, "nixd/ipc/textDocument/rename", Params,
            1e6);
    auto Edits = latestMatchOr<std::vector<lspserver::TextEdit>>(
        Responses,
        [](const std::vector<lspserver::TextEdit> &) -> bool { return true; });
    std::map<std::string, std::vector<lspserver::TextEdit>> Changes;
    Changes[Params.textDocument.uri.uri()] = std::move(Edits);
    Reply(lspserver::WorkspaceEdit{.changes = std::move(Changes)});
  };

  boost::asio::post(Pool, std::move(Task));
}

void Server::onPrepareRename(
    const lspserver::TextDocumentPositionParams &Params,
    lspserver::Callback<llvm::json::Value> Reply) {
  auto Task = [=, Reply = std::move(Reply), this]() mutable {
    auto Responses =
        askWorkers<lspserver::RenameParams, std::vector<lspserver::TextEdit>>(
            EvalWorkers, EvalWorkerLock, "nixd/ipc/textDocument/rename",
            lspserver::RenameParams{
                .textDocument = Params.textDocument,
                .position = Params.position,
            },
            1e6);
    auto Edits = latestMatchOr<std::vector<lspserver::TextEdit>>(
        Responses,
        [](const std::vector<lspserver::TextEdit> &) -> bool { return true; });
    for (const auto &Edit : Edits) {
      if (Edit.range.contains(Params.position)) {
        Reply(Edit.range);
        return;
      }
    }
    Reply(lspserver::error("no rename edits available"));
  };

  boost::asio::post(Pool, std::move(Task));
}

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

void Server::onEvalDiagnostic(const ipc::Diagnostics &Diag) {
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

void Server::onEvalFinished(const ipc::WorkerMessage &) {
  EvalDoneSmp.release();
}

void Server::onFormat(
    const lspserver::DocumentFormattingParams &Params,
    lspserver::Callback<std::vector<lspserver::TextEdit>> Reply) {

  auto Task = [=, Reply = std::move(Reply), this]() mutable {
    lspserver::PathRef File = Params.textDocument.uri.file();
    auto Code = *getDraft(File);
    auto FormatFuture = std::async([Code = std::move(Code),
                                    this]() -> std::optional<std::string> {
      try {
        namespace bp = boost::process;
        bp::opstream To;
        bp::ipstream From;
        bp::child Fmt(Config.getFormatCommand(), bp::std_out > From,
                      bp::std_in < To);

        To << Code;
        To.flush();

        To.pipe().close();

        std::string FormattedCode;
        while (From.good() && !From.eof()) {
          std::string Buf;
          std::getline(From, Buf, {});
          FormattedCode += Buf;
        }
        Fmt.wait();
        return FormattedCode;
      } catch (std::exception &E) {
        lspserver::elog(
            "cannot summon external formatting command, reason: {0}", E.what());
      } catch (...) {
      }
      return std::nullopt;
    });

    /// Wait for the external command, if this timeout, something went wrong
    auto Status = FormatFuture.wait_for(std::chrono::seconds(1));
    std::optional<std::string> FormattedCode;
    using lspserver::TextEdit;
    if (Status == std::future_status::ready)
      FormattedCode = FormatFuture.get();

    if (FormattedCode.has_value()) {
      TextEdit E{{{0, 0}, {INT_MAX, INT_MAX}}, FormattedCode.value()};
      Reply(std::vector{E});
    } else {
      Reply(lspserver::error("no formatting response received"));
    }
  };
  boost::asio::post(Pool, std::move(Task));
}
} // namespace nixd
