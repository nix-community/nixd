#include "nixd-config.h"

#include "nixd/AST/AttrLocator.h"
#include "nixd/AST/ParseAST.h"
#include "nixd/Expr/Expr.h"
#include "nixd/Parser/Parser.h"
#include "nixd/Parser/Require.h"
#include "nixd/Server/ASTManager.h"
#include "nixd/Server/ConfigSerialization.h"
#include "nixd/Server/Controller.h"
#include "nixd/Server/EvalDraftStore.h"
#include "nixd/Server/IPCSerialization.h"
#include "nixd/Support/Diagnostic.h"
#include "nixd/Support/Support.h"

#include "lspserver/Connection.h"
#include "lspserver/DraftStore.h"
#include "lspserver/Logger.h"
#include "lspserver/Path.h"
#include "lspserver/Protocol.h"
#include "lspserver/SourceCode.h"
#include "lspserver/URI.h"

#include <nix/util.hh>

#include <llvm/ADT/FunctionExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/ScopedPrinter.h>
#include <llvm/Support/raw_ostream.h>

#include <boost/iostreams/stream.hpp>
#include <boost/process.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <semaphore>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <variant>

#include <unistd.h>

namespace nixd {

using lspserver::DraftStore;

std::unique_ptr<Controller::Proc>
Controller::forkWorker(llvm::unique_function<void()> WorkerAction) {
  auto To = std::make_unique<nix::Pipe>();
  auto From = std::make_unique<nix::Pipe>();

  To->create();
  From->create();

  auto ForkPID = fork();
  if (ForkPID == -1) {
    throw std::system_error(
        std::make_error_code(static_cast<std::errc>(errno)));
  }

  if (ForkPID == 0) {
    // Redirect stdin & stdout to our pipes, instead of LSP clients
    dup2(To->readSide.get(), 0);
    dup2(From->writeSide.get(), 1);
    WorkerAction();
    abort();
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

    return std::unique_ptr<Proc>(
        new Proc{.ToPipe = std::move(To),
                 .FromPipe = std::move(From),
                 .OutPort = std::move(OutPort),
                 .OwnedStream = std::move(ProcFdStream),
                 .Pid = ForkPID,
                 .InputDispatcher = std::move(WorkerInputDispatcher),
                 .Smp = std::ref(FinishSmp),
                 .WaitWorker = WaitWorker});
  }
}

void Controller::syncDrafts(Proc &WorkerProc) {
  using OP = lspserver::DidOpenTextDocumentParams;
  using TI = lspserver::TextDocumentItem;
  auto SendIPCDocumentOpen = mkOutNotifiction<OP>(
      "nixd/ipc/textDocument/didOpen", WorkerProc.OutPort.get());
  for (const auto &Path : DraftMgr.getActiveFiles()) {
    if (auto Draft = DraftMgr.getDraft(Path)) {
      const auto &[Content, Version] = *Draft;
      SendIPCDocumentOpen(OP{TI{
          .uri = lspserver::URIForFile::canonicalize(Path, Path),
          .version = EvalDraftStore::decodeVersion(Version),
          .text = *Content,
      }});
    }
  }
}

std::unique_ptr<Controller::Proc> Controller::createEvalWorker() {
  auto Args = nix::Strings{"nixd", "-role=evaluator"};
  auto EvalWorker = selfExec(nix::stringsToCharPtrs(Args).data());
  syncDrafts(*EvalWorker);
  auto AskEval = mkOutNotifiction<ipc::EvalParams>("nixd/ipc/eval",
                                                   EvalWorker->OutPort.get());
  {
    std::lock_guard _(ConfigLock);
    AskEval(ipc::EvalParams{WorkspaceVersion, Config.eval});
  }
  return EvalWorker;
}

std::unique_ptr<Controller::Proc> Controller::createOptionWorker() {
  auto Args = nix::Strings{"nixd", "-role=option"};
  auto Worker = selfExec(nix::stringsToCharPtrs(Args).data());
  auto AskEval = mkOutNotifiction<configuration::TopLevel::Options>(
      "nixd/ipc/evalOptionSet", Worker->OutPort.get());
  {
    std::lock_guard _(ConfigLock);
    AskEval(Config.options);
  }
  return Worker;
}

bool Controller::createEnqueueEvalWorker() {
  size_t Size;
  {
    std::lock_guard _(ConfigLock);
    Size = Config.eval.workers;
  }
  return enqueueWorker(EvalWorkerLock, EvalWorkers, createEvalWorker(), Size);
}

bool Controller::createEnqueueOptionWorker() {
  return enqueueWorker(OptionWorkerLock, OptionWorkers, createOptionWorker(),
                       1);
}

bool Controller::enqueueWorker(WorkerContainerLock &Lock,
                               WorkerContainer &Container,
                               std::unique_ptr<Proc> Worker, size_t Size) {
  std::lock_guard _(Lock);
  Container.emplace_back(std::move(Worker));
  if (Container.size() > Size) {
    Container.pop_front();
    return true;
  }
  return false;
}

void Controller::addDocument(lspserver::PathRef File, llvm::StringRef Contents,
                             llvm::StringRef Version) {
  using namespace lspserver;
  WorkspaceVersion++;
  auto IVersion = DraftStore::decodeVersion(Version);
  // Since this file is updated, we first clear its diagnostic
  PublishDiagnosticsParams Notification;
  Notification.uri = URIForFile::canonicalize(File, File);
  Notification.diagnostics = {};
  Notification.version = IVersion;
  PublishDiagnostic(Notification);

  DraftMgr.addDraft(File, Version, Contents);
  ASTMgr.schedParse(Contents.str(), File.str(), IVersion.value_or(0));
  createEnqueueEvalWorker();
}

void Controller::updateConfig(configuration::TopLevel &&NewConfig) {
  {
    std::lock_guard _(ConfigLock);
    Config = std::move(NewConfig);
  }
  createEnqueueEvalWorker();
  createEnqueueOptionWorker();
}

void Controller::fetchConfig() {
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
Controller::parseConfig(llvm::StringRef JSON) {
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

void Controller::readJSONConfig(lspserver::PathRef File) noexcept {
  try {
    std::string ConfigStr;
    std::ostringstream SS;
    std::ifstream In(File.str(), std::ios::in);
    SS << In.rdbuf();

    if (auto NewConfig = parseConfig(SS.str())) {
      JSONConfig = std::move(NewConfig.get());
    } else {
      throw nix::Error("configuration cannot be parsed");
    }
  } catch (std::exception &E) {
  } catch (...) {
  }
}

std::shared_ptr<const std::string>
Controller::getDraft(lspserver::PathRef File) const {
  auto Draft = DraftMgr.getDraft(File);
  if (!Draft)
    return nullptr;
  return std::move(Draft->Contents);
}

Controller::Controller(std::unique_ptr<lspserver::InboundPort> In,
                       std::unique_ptr<lspserver::OutboundPort> Out,
                       int WaitWorker)
    : LSPServer(std::move(In), std::move(Out)), WaitWorker(WaitWorker),
      ASTMgr(Pool) {

  // JSON Config, run before initialize
  readJSONConfig();
  configuration::TopLevel JSONConfigCopy = JSONConfig;
  updateConfig(std::move(JSONConfigCopy));
  WorkspaceConfiguration =
      mkOutMethod<lspserver::ConfigurationParams, configuration::TopLevel>(
          "workspace/configuration", nullptr, JSONConfig);

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

  // Language Features
  Registry.addMethod("textDocument/documentLink", this,
                     &Controller::onDocumentLink);
  Registry.addMethod("textDocument/documentSymbol", this,
                     &Controller::onDocumentSymbol);
  Registry.addMethod("textDocument/hover", this, &Controller::onHover);
  Registry.addMethod("textDocument/completion", this,
                     &Controller::onCompletion);
  Registry.addMethod("textDocument/declaration", this,
                     &Controller::onDecalration);
  Registry.addMethod("textDocument/definition", this,
                     &Controller::onDefinition);
  Registry.addMethod("textDocument/formatting", this, &Controller::onFormat);
  Registry.addMethod("textDocument/rename", this, &Controller::onRename);
  Registry.addMethod("textDocument/prepareRename", this,
                     &Controller::onPrepareRename);

  PublishDiagnostic = mkOutNotifiction<lspserver::PublishDiagnosticsParams>(
      "textDocument/publishDiagnostics");

  // Workspace
  Registry.addNotification("workspace/didChangeConfiguration", this,
                           &Controller::onWorkspaceDidChangeConfiguration);

  /// IPC
  Registry.addNotification("nixd/ipc/diagnostic", this,
                           &Controller::onEvalDiagnostic);

  Registry.addNotification("nixd/ipc/finished", this, &Controller::onFinished);
}

//-----------------------------------------------------------------------------/
// Life Cycle

void Controller::onInitialize(
    const lspserver::InitializeParams &InitializeParams,
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
      {"documentLinkProvider", llvm::json::Object{{"resolveProvider", false}}},
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

void Controller::onDocumentDidOpen(
    const lspserver::DidOpenTextDocumentParams &Params) {
  lspserver::PathRef File = Params.textDocument.uri.file();

  const std::string &Contents = Params.textDocument.text;

  addDocument(File, Contents,
              DraftStore::encodeVersion(Params.textDocument.version));
}

void Controller::onDocumentDidChange(
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
  addDocument(File, NewCode,
              DraftStore::encodeVersion(Params.textDocument.version));
}

void Controller::onDocumentDidClose(
    const lspserver::DidCloseTextDocumentParams &Params) {
  lspserver::PathRef File = Params.textDocument.uri.file();
  auto Code = getDraft(File);
  removeDocument(File);
}

//-----------------------------------------------------------------------------/
// Language Features

void Controller::onDecalration(
    const lspserver::TextDocumentPositionParams &Params,
    lspserver::Callback<llvm::json::Value> Reply) {
  using llvm::json::Value;

  bool EnableOption;
  {
    std::lock_guard _(ConfigLock);
    EnableOption = Config.options.enable;
  }
  if (!EnableOption) {
    Reply(nullptr);
    return;
  }

  auto Action = [=, this](ReplyRAII<Value> &&RR, const ParseAST &AST,
                          ASTManager::VersionTy Version) mutable {
    // Set the default response to "null", instead of errors
    RR.Response = nullptr;

    AttrLocator Locator(AST);

    auto AttrPath = AST.getAttrPath(Locator.locate(Params.position));
    ipc::AttrPathParams APParams;
    APParams.Path = std::move(AttrPath);

    auto Responses = askWC<lspserver::Location>(
        "nixd/ipc/option/textDocument/declaration", APParams,
        {OptionWorkers, OptionWorkerLock, 2e4});

    if (Responses.empty())
      return;

    RR.Response = std::move(Responses.back());
  };
  std::string Path = Params.textDocument.uri.file().str();
  withParseAST<Value>({std::move(Reply)}, Path, std::move(Action));
}

void Controller::onDefinition(
    const lspserver::TextDocumentPositionParams &Params,
    lspserver::Callback<llvm::json::Value> Reply) {
  using RTy = lspserver::Location;
  using namespace lspserver;
  using V = llvm::json::Value;
  using O = llvm::json::Object;

  auto Task = [=, Reply = std::move(Reply), this]() mutable {
    // Firstly, ask the eval workers if there is a suitable location
    // Prefer evaluated locations because in most case this is more useful
    constexpr auto Method = "nixd/ipc/textDocument/definition";
    auto Resp =
        askWC<RTy>(Method, Params, WC{EvalWorkers, EvalWorkerLock, 1e6});
    if (!Resp.empty()) {
      Reply(latestMatchOr<RTy, V>(
          Resp, [](const RTy &Location) -> bool { return true; }, O{}));
      return;
    }

    // Otherwise, statically find the definition
    const auto &URI = Params.textDocument.uri;
    auto Path = URI.file().str();
    const auto &Pos = Params.position;

    auto Action = [=](ReplyRAII<llvm::json::Value> &&RR, const ParseAST &AST,
                      ASTManager::VersionTy Version) {
      try {
        Location L;
        auto Def = AST.def(Pos);
        L.range = AST.defRange(Def);
        L.uri = URI;
        RR.Response = std::move(L);
      } catch (std::exception &E) {
        // Reply an error is annoying at the user interface, let's just log.
        auto ErrMsg = stripANSI(E.what());
        RR.Response = O{};
        elog("static definition: {0}", std::move(ErrMsg));
      }
    };

    withParseAST<V>({std::move(Reply)}, Path, std::move(Action));
  };

  boost::asio::post(Pool, std::move(Task));
}

void Controller::onDocumentLink(
    const lspserver::DocumentLinkParams &Params,
    lspserver::Callback<std::vector<lspserver::DocumentLink>> Reply) {
  auto Task = [=, Reply = std::move(Reply), this]() mutable {
    auto Path = Params.textDocument.uri.file().str();
    auto Action = [File = Path](ReplyRAII<ParseAST::Links> &&RR,
                                const ParseAST &AST,
                                ASTManager::VersionTy Version) {
      RR.Response = AST.documentLink(File);
    };
    auto RR = ReplyRAII<ParseAST::Links>(std::move(Reply));
    withParseAST<ParseAST::Links>(std::move(RR), Path, std::move(Action));
  };

  boost::asio::post(Pool, std::move(Task));
}

void Controller::onDocumentSymbol(
    const lspserver::DocumentSymbolParams &Params,
    lspserver::Callback<std::vector<lspserver::DocumentSymbol>> Reply) {

  auto Task = [=, Reply = std::move(Reply), this]() mutable {
    auto Action = [](ReplyRAII<ParseAST::Symbols> &&RR, const ParseAST &AST,
                     ASTManager::VersionTy Version) {
      RR.Response = AST.documentSymbol();
    };
    auto RR = ReplyRAII<ParseAST::Symbols>(std::move(Reply));
    auto Path = Params.textDocument.uri.file().str();
    withParseAST<ParseAST::Symbols>(std::move(RR), Path, std::move(Action));
  };

  boost::asio::post(Pool, std::move(Task));
}

void Controller::onHover(const lspserver::TextDocumentPositionParams &Params,
                         lspserver::Callback<lspserver::Hover> Reply) {
  using RTy = lspserver::Hover;
  constexpr auto Method = "nixd/ipc/textDocument/hover";
  auto Task = [=, Reply = std::move(Reply), this]() mutable {
    auto Resp =
        askWC<RTy>(Method, Params, WC{EvalWorkers, EvalWorkerLock, 2e6});
    Reply(latestMatchOr<lspserver::Hover>(Resp, [](const lspserver::Hover &H) {
      return H.contents.value.length() != 0;
    }));
  };

  boost::asio::post(Pool, std::move(Task));
}

static llvm::StringRef getAttrPathFromCode(llvm::StringRef Code,
                                           lspserver::Position Position) {
  auto ExpectedOffset = positionToOffset(Code, Position);
  auto Offset = ExpectedOffset.get();
  // get the attr path
  auto TruncateBackCode = Code.substr(0, Offset);
  auto [_, AttrPath] = TruncateBackCode.rsplit(" ");
  return AttrPath;
}

void Controller::onCompletion(const lspserver::CompletionParams &Params,
                              lspserver::Callback<llvm::json::Value> Reply) {
  // Statically construct the completion list.
  auto Path = Params.textDocument.uri.file();

  auto OpDraft = DraftMgr.getDraft(Path);
  if (!OpDraft) {
    Reply(lspserver::error("requested completion list on unknown draft path"));
    return;
  }

  auto Action = [Params, Reply = std::move(Reply), Draft = *OpDraft,
                 this](const ParseAST &AST,
                       ASTManager::VersionTy Version) mutable {
    using PL = ParseAST::LocationContext;
    using List = lspserver::CompletionList;
    ReplyRAII<llvm::json::Value> RR(std::move(Reply));

    auto CompletionFromOptions = [&, Draft, this]() -> std::optional<List> {
      {
        std::lock_guard _(ConfigLock);
        if (!Config.options.enable)
          return std::nullopt;
      }

      auto Position = Params.position;
      auto TriggerCharacter = Params.context.triggerCharacter;
      std::vector<std::string> AttrPath;

      // Nested level, e.g.
      // {
      //   foo = {
      //    bar = { | };
      //   };       ^  <- [ "foo", "bar" ]
      // }
      auto NestedAttrPath = AST.getAttrPath(AST.lookupContainMin(Position));

      // "Code"AttrPath, extracted by string manipulation (not parsing)
      // e.g. : { foo.bar.b|  }
      //          ^~~~~~~~~~     <- [ "foo", "bar", "b" ]
      auto Code = llvm::StringRef(*Draft.Contents);
      auto AttrPathStr = getAttrPathFromCode(Code, Position);
      llvm::SmallVector<llvm::StringRef> CodeAttrPath;
      AttrPathStr.split(CodeAttrPath, ".");

      // Pops the last item, it is incomplete
      // {
      //   boot.isContain
      // }      ^~~~~~~~~  pops this
      if (!CodeAttrPath.empty() && TriggerCharacter != ".")
        CodeAttrPath.pop_back();

      // Merge "nested" and "code" attrpath, for completion
      AttrPath = std::move(NestedAttrPath);
      for (const auto &Attr : CodeAttrPath)
        AttrPath.emplace_back(Attr.str());

      ipc::AttrPathParams APParams;
      APParams.Path = std::move(AttrPath);

      auto Resp = askWC<lspserver::CompletionList>(
          "nixd/ipc/textDocument/completion/options", APParams,
          {OptionWorkers, OptionWorkerLock, 1e5});
      if (!Resp.empty())
        return Resp.back();
      return std::nullopt;
    };

    auto CompletionFromEval = [&, this]() -> std::optional<List> {
      constexpr auto Method = "nixd/ipc/textDocument/completion";
      auto Resp =
          askWC<List>(Method, Params, {EvalWorkers, EvalWorkerLock, 2e6});
      std::optional<List> R;
      if (!Resp.empty())
        return std::move(Resp.back());
      return std::nullopt;
    };

    switch (AST.getContext(Params.position)) {
    case (PL::AttrName): {
      // Requested completion on an attribute name, not values.
      RR.Response = CompletionFromOptions();
      break;
    }
    case (PL::Value): {
      RR.Response = CompletionFromEval();
      break;
    }
    case (PL::Unknown):
      List L;
      if (auto LOptions = CompletionFromOptions())
        L.items.insert(L.items.end(), (*LOptions).items.begin(),
                       (*LOptions).items.end());
      if (auto LEval = CompletionFromEval())
        L.items.insert(L.items.end(), (*LEval).items.begin(),
                       (*LEval).items.end());
      L.isIncomplete = true;
      RR.Response = std::move(L);
    }
  };

  auto Version = EvalDraftStore::decodeVersion(OpDraft->Version).value_or(0);
  ASTMgr.withAST(Path.str(), Version, std::move(Action));
}

void Controller::onRename(const lspserver::RenameParams &Params,
                          lspserver::Callback<lspserver::WorkspaceEdit> Reply) {

  auto Task = [Params, Reply = std::move(Reply), this]() mutable {
    auto URI = Params.textDocument.uri;
    auto Path = URI.file().str();
    auto Action = [URI, Params](ReplyRAII<lspserver::WorkspaceEdit> &&RR,
                                const ParseAST &AST,
                                ASTManager::VersionTy Version) {
      std::map<std::string, std::vector<lspserver::TextEdit>> Changes;
      if (auto Edits = AST.rename(Params.position, Params.newName)) {
        Changes[URI.uri()] = std::move(*Edits);
        RR.Response = lspserver::WorkspaceEdit{.changes = Changes};
        return;
      }
      RR.Response = lspserver::error("no rename edits available");
      return;
    };
    withParseAST<lspserver::WorkspaceEdit>(
        ReplyRAII<lspserver::WorkspaceEdit>(std::move(Reply)), Path,
        std::move(Action));
  };

  boost::asio::post(Pool, std::move(Task));
}

void Controller::onPrepareRename(
    const lspserver::TextDocumentPositionParams &Params,
    lspserver::Callback<llvm::json::Value> Reply) {
  auto Task = [Params, Reply = std::move(Reply), this]() mutable {
    auto Path = Params.textDocument.uri.file().str();
    auto Action = [Params](ReplyRAII<llvm::json::Value> &&RR,
                           const ParseAST &AST, ASTManager::VersionTy Version) {
      auto Pos = Params.position;
      if (auto Edits = AST.rename(Pos, "")) {
        for (const auto &Edit : *Edits) {
          if (Edit.range.contains(Pos)) {
            RR.Response = Edit.range;
            return;
          }
        }
      }
      RR.Response = lspserver::error("no rename edits available");
      return;
    };
    withParseAST<llvm::json::Value>(
        ReplyRAII<llvm::json::Value>(std::move(Reply)), Path,
        std::move(Action));
  };

  boost::asio::post(Pool, std::move(Task));
}

void Controller::clearDiagnostic(lspserver::PathRef Path) {
  lspserver::URIForFile Uri = lspserver::URIForFile::canonicalize(Path, Path);
  clearDiagnostic(Uri);
}

void Controller::clearDiagnostic(const lspserver::URIForFile &FileUri) {
  lspserver::PublishDiagnosticsParams Notification;
  Notification.uri = FileUri;
  Notification.diagnostics = {};
  PublishDiagnostic(Notification);
}

void Controller::onEvalDiagnostic(const ipc::Diagnostics &Diag) {
  lspserver::log("received diagnostic from worker: {0}", Diag.WorkspaceVersion);
  auto Defer = std::shared_ptr<void>(nullptr, [this, Diag](...) {
    onFinished(ipc::WorkerMessage{Diag.WorkspaceVersion});
  });
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

void Controller::onFinished(const ipc::WorkerMessage &) { FinishSmp.release(); }

void Controller::onFormat(
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

        std::string FormatCommand;
        {
          std::lock_guard _(ConfigLock);
          FormatCommand = Config.formatting.command;
        }

        bp::child Fmt(std::move(FormatCommand), bp::std_out > From,
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
