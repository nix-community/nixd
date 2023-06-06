#pragma once

#include "nixd/EvalDraftStore.h"
#include "nixd/JSONSerialization.h"

#include "lspserver/Connection.h"
#include "lspserver/DraftStore.h"
#include "lspserver/Function.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Logger.h"
#include "lspserver/Path.h"
#include "lspserver/SourceCode.h"

#include <llvm/ADT/FunctionExtras.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>

namespace nixd {

struct CompletionHelper {
  using Items = std::vector<lspserver::CompletionItem>;
  static Items fromEnvRecursive(const nix::SymbolTable &STable,
                                const nix::StaticEnv &SEnv,
                                const nix::Env &NixEnv);
  static Items fromEnvWith(const nix::SymbolTable &STable,
                           const nix::Env &NixEnv);
  static Items fromStaticEnv(const nix::SymbolTable &STable,
                             const nix::StaticEnv &SEnv);
};

/// The server instance, nix-related language features goes here
class Server : public lspserver::LSPServer {
public:
  using WorkspaceVersionTy = ipc::WorkspaceVersionTy;

  struct Proc {
    std::unique_ptr<nix::Pipe> ToPipe;
    std::unique_ptr<nix::Pipe> FromPipe;
    std::unique_ptr<lspserver::OutboundPort> OutPort;
    std::unique_ptr<llvm::raw_ostream> OwnedStream;

    nix::Pid Pid;
    WorkspaceVersionTy WorkspaceVersion;
    std::thread InputDispatcher;

    [[nodiscard]] nix::AutoCloseFD to() const {
      return ToPipe->writeSide.get();
    };
    [[nodiscard]] nix::AutoCloseFD from() const {
      return FromPipe->readSide.get();
    };
  };

  enum class ServerRole {
    /// Parent process of the server
    Controller,
    /// Child process
    Evaluator
  };

  template <class ReplyTy> struct ReplyRAII {
    lspserver::Callback<ReplyTy> R;
    llvm::Expected<ReplyTy> Response =
        lspserver::error("no response available");
    ReplyRAII(decltype(R) R) : R(std::move(R)) {}
    ~ReplyRAII() { R(std::move(Response)); };
  };

private:
  int WaitWorker = 0;

  ServerRole Role = ServerRole::Controller;

  WorkspaceVersionTy WorkspaceVersion = 1;

  std::deque<std::unique_ptr<Proc>> Workers;

  EvalDraftStore DraftMgr;

  lspserver::ClientCapabilities ClientCaps;

  configuration::TopLevel Config;

  std::shared_ptr<const std::string> getDraft(lspserver::PathRef File) const;

  void addDocument(lspserver::PathRef File, llvm::StringRef Contents,
                   llvm::StringRef Version);

  void removeDocument(lspserver::PathRef File) {
    DraftMgr.removeDraft(File);
    updateWorkspaceVersion(File);
  }

  /// LSP defines file versions as numbers that increase.
  /// treats them as opaque and therefore uses strings instead.
  static std::string encodeVersion(std::optional<int64_t> LSPVersion);

  llvm::unique_function<void(const lspserver::PublishDiagnosticsParams &)>
      PublishDiagnostic;

  llvm::unique_function<void(const lspserver::ConfigurationParams &,
                             lspserver::Callback<configuration::TopLevel>)>
      WorkspaceConfiguration;

  std::mutex DiagStatusLock;
  struct DiagnosticStatus {
    std::vector<lspserver::PublishDiagnosticsParams> ClientDiags;
    WorkspaceVersionTy WorkspaceVersion = 0;
  } DiagStatus; // GUARDED_BY(DiagStatusLock)

  //---------------------------------------------------------------------------/
  // Controller
  void onWorkerDiagnostic(const ipc::Diagnostics &);

  template <class Arg, class Resp>
  auto askWorkers(const std::deque<std::unique_ptr<Server::Proc>> &Workers,
                  llvm::StringRef IPCMethod, const Arg &Params,
                  unsigned Timeout) {
    // For all active workers, send the completion request
    auto ListStoreOptional = std::make_shared<std::vector<std::optional<Resp>>>(
        Workers.size(), std::nullopt);
    auto ListStoreLock = std::make_shared<std::mutex>();

    size_t I = 0;
    for (const auto &Worker : Workers) {
      auto Request = mkOutMethod<Arg, Resp>(IPCMethod, Worker->OutPort.get());
      Request(Params, [I, ListStoreOptional,
                       ListStoreLock](llvm::Expected<Resp> Result) {
        // The worker answered our request, fill the completion
        // lists then.
        if (Result) {
          std::lock_guard Guard(*ListStoreLock);
          (*ListStoreOptional)[I] = Result.get();
        } else {
          lspserver::elog("worker {0} reported error: {1}", I,
                          Result.takeError());
        }
      });
      I++;
    }

    usleep(Timeout);

    std::lock_guard Guard(*ListStoreLock);
    std::vector<Resp> AnsweredResp;
    AnsweredResp.reserve(ListStoreOptional->size());
    // Then, filter out un-answered response
    for (const auto &R : *ListStoreOptional) {
      if (R.has_value())
        AnsweredResp.push_back(*R);
    }

    return AnsweredResp;
  }

  //---------------------------------------------------------------------------/
  // Worker members
  llvm::unique_function<void(const ipc::Diagnostics &)> WorkerDiagnostic;

  std::unique_ptr<IValueEvalResult> IER;

public:
  Server(std::unique_ptr<lspserver::InboundPort> In,
         std::unique_ptr<lspserver::OutboundPort> Out, int WaitWorker = 0);

  ~Server() override { usleep(WaitWorker); }

  template <class ReplyTy>
  void
  withAST(const std::string &RequestedFile, ReplyRAII<ReplyTy> RR,
          llvm::unique_function<void(nix::ref<EvalAST>, ReplyRAII<ReplyTy> &&)>
              Action) {
    try {
      auto AST = IER->Forest.at(RequestedFile);
      Action(AST, std::move(RR));
    } catch (std::out_of_range &E) {
      RR.Response = lspserver::error("no AST available on requested file {0}",
                                     RequestedFile);
    }
  }

  void eval(lspserver::PathRef File, int Depth);

  void updateWorkspaceVersion(lspserver::PathRef File);

  void switchToEvaluator(lspserver::PathRef File);

  void fetchConfig();

  void clearDiagnostic(lspserver::PathRef Path);

  void clearDiagnostic(const lspserver::URIForFile &FileUri);

  static lspserver::PublishDiagnosticsParams
  diagNixError(lspserver::PathRef Path, const nix::BaseError &NixErr,
               std::optional<int64_t> Version);

  //---------------------------------------------------------------------------/
  // Life Cycle

  void onInitialize(const lspserver::InitializeParams &,
                    lspserver::Callback<llvm::json::Value>);

  void onInitialized(const lspserver::InitializedParams &Params) {
    fetchConfig();
  }

  //---------------------------------------------------------------------------/
  // Text Document Synchronization

  void onDocumentDidOpen(const lspserver::DidOpenTextDocumentParams &Params);

  void
  onDocumentDidChange(const lspserver::DidChangeTextDocumentParams &Params);

  void onDocumentDidClose(const lspserver::DidCloseTextDocumentParams &Params);

  //---------------------------------------------------------------------------/
  // Language Features

  void publishStandaloneDiagnostic(lspserver::URIForFile Uri,
                                   std::string Content,
                                   std::optional<int64_t> LSPVersion);

  // Controller Methods

  void onDefinition(const lspserver::TextDocumentPositionParams &,
                    lspserver::Callback<llvm::json::Value>);

  void onHover(const lspserver::TextDocumentPositionParams &,
               lspserver::Callback<lspserver::Hover>);

  void onCompletion(const lspserver::CompletionParams &,
                    lspserver::Callback<lspserver::CompletionList>);

  void onFormat(const lspserver::DocumentFormattingParams &,
                lspserver::Callback<std::vector<lspserver::TextEdit>>);

  // Worker

  void onWorkerDefinition(const lspserver::TextDocumentPositionParams &,
                          lspserver::Callback<lspserver::Location>);

  void onWorkerHover(const lspserver::TextDocumentPositionParams &,
                     lspserver::Callback<llvm::json::Value>);

  void onWorkerCompletion(const lspserver::CompletionParams &,
                          lspserver::Callback<llvm::json::Value>);

  //---------------------------------------------------------------------------/
  // Workspace Features

  void onWorkspaceDidChangeConfiguration(
      const lspserver::DidChangeConfigurationParams &) {
    fetchConfig();
  }
};

}; // namespace nixd
