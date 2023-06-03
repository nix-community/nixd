#pragma once

#include "nixd/EvalDraftStore.h"

#include "lspserver/Connection.h"
#include "lspserver/DraftStore.h"
#include "lspserver/Function.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Logger.h"
#include "lspserver/Path.h"
#include "lspserver/Protocol.h"
#include "lspserver/SourceCode.h"

#include <llvm/ADT/FunctionExtras.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <memory>

// Extension to `lspserver`
namespace lspserver {

llvm::json::Value toJSON(const CompletionContext &R);

llvm::json::Value toJSON(const TextDocumentPositionParams &R);

llvm::json::Value toJSON(const CompletionParams &R);

bool fromJSON(const llvm::json::Value &Params, CompletionItem &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, CompletionList &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, Hover &R, llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, MarkupContent &R,
              llvm::json::Path P);

} // namespace lspserver

namespace nixd {

namespace configuration {

struct InstallableConfigurationItem {
  std::vector<std::string> args;
  std::string installable;
};

struct TopLevel {
  /// Nix installables that will be used for root translation unit.
  std::optional<InstallableConfigurationItem> installable;

  /// Get installable arguments specified in this config, fallback to file \p
  /// Fallback if 'installable' is not set.
  [[nodiscard]] std::tuple<nix::Strings, std::string>
  getInstallable(std::string Fallback) const;
};

bool fromJSON(const llvm::json::Value &Params, TopLevel &R, llvm::json::Path P);
bool fromJSON(const llvm::json::Value &Params, InstallableConfigurationItem &R,
              llvm::json::Path P);
} // namespace configuration

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

using WorkspaceVersionTy = uint64_t;

namespace ipc {

/// Messages sent by workers must tell it's version
struct WorkerMessage {
  WorkspaceVersionTy WorkspaceVersion;
};

bool fromJSON(const llvm::json::Value &, WorkerMessage &, llvm::json::Path);
llvm::json::Value toJSON(const WorkerMessage &);

/// Sent by the worker process, tell us it has prepared diagnostics
/// <----
struct Diagnostics : WorkerMessage {
  std::vector<lspserver::PublishDiagnosticsParams> Params;
};

bool fromJSON(const llvm::json::Value &, lspserver::PublishDiagnosticsParams &,
              llvm::json::Path);

bool fromJSON(const llvm::json::Value &, Diagnostics &, llvm::json::Path);
llvm::json::Value toJSON(const Diagnostics &);

struct Completion : WorkerMessage {
  lspserver::CompletionList List;
};

} // namespace ipc

/// The server instance, nix-related language features goes here
class Server : public lspserver::LSPServer {

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
    updateWorkspaceVersion();
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

  void eval(const std::string &Fallback);

  void updateWorkspaceVersion();

  void switchToEvaluator();

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
