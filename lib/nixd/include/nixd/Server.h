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

  using WorkspaceVersionTy = ipc::WorkspaceVersionTy;

  int WaitWorker = 0;

  enum class ServerRole {
    /// Parent process of the server
    Controller,
    /// Child process
    Evaluator
  } Role = ServerRole::Controller;

  WorkspaceVersionTy WorkspaceVersion = 1;

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

  //---------------------------------------------------------------------------/
  // Worker members
  llvm::unique_function<void(const ipc::Diagnostics &)> WorkerDiagnostic;

  std::unique_ptr<IValueEvalResult> IER;

public:
  Server(std::unique_ptr<lspserver::InboundPort> In,
         std::unique_ptr<lspserver::OutboundPort> Out, int WaitWorker = 0);

  ~Server() override { usleep(WaitWorker); }

  void eval(lspserver::PathRef File, int Depth);

  void updateWorkspaceVersion(lspserver::PathRef File);

  void switchToEvaluator(lspserver::PathRef File);

  void fetchConfig();

  void clearDiagnostic(lspserver::PathRef Path);

  void clearDiagnostic(const lspserver::URIForFile &FileUri);

  static lspserver::PublishDiagnosticsParams
  diagNixError(lspserver::PathRef Path, const nix::BaseError &NixErr,
               std::optional<int64_t> Version);

  void onInitialize(const lspserver::InitializeParams &,
                    lspserver::Callback<llvm::json::Value>);

  void onInitialized(const lspserver::InitializedParams &Params) {
    fetchConfig();
  }

  void onDocumentDidOpen(const lspserver::DidOpenTextDocumentParams &Params);

  void
  onDocumentDidChange(const lspserver::DidChangeTextDocumentParams &Params);

  void publishStandaloneDiagnostic(lspserver::URIForFile Uri,
                                   std::string Content,
                                   std::optional<int64_t> LSPVersion);

  void onWorkspaceDidChangeConfiguration(
      const lspserver::DidChangeConfigurationParams &) {
    fetchConfig();
  }

  void onDefinition(const lspserver::TextDocumentPositionParams &,
                    lspserver::Callback<llvm::json::Value>);

  void onWorkerDefinition(const lspserver::TextDocumentPositionParams &,
                          lspserver::Callback<lspserver::Location>);

  void onHover(const lspserver::TextDocumentPositionParams &,
               lspserver::Callback<lspserver::Hover>);
  void onWorkerHover(const lspserver::TextDocumentPositionParams &,
                     lspserver::Callback<llvm::json::Value>);

  void onCompletion(const lspserver::CompletionParams &,
                    lspserver::Callback<lspserver::CompletionList>);

  void onWorkerCompletion(const lspserver::CompletionParams &,
                          lspserver::Callback<llvm::json::Value>);
};

}; // namespace nixd
