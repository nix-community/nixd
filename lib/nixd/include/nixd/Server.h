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
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <semaphore>
#include <stdexcept>

#include <pthread.h>

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

    std::counting_semaphore<> &EvalDoneSmp;

    [[nodiscard]] nix::AutoCloseFD to() const {
      return ToPipe->writeSide.get();
    };
    [[nodiscard]] nix::AutoCloseFD from() const {
      return FromPipe->readSide.get();
    };

    ~Proc() {
      auto Th = InputDispatcher.native_handle();
      pthread_cancel(Th);
      InputDispatcher.join();

      // Tell that we finished (being killed).
      // EvalDoneSmp.release();
    }
  };

  enum class ServerRole {
    /// Parent process of the server
    Controller,
    /// Child process
    Evaluator,
    OptionProvider
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

  // Evaluator notify us "evaluation done" by incresing this semaphore.
  std::counting_semaphore<> EvalDoneSmp = std::counting_semaphore(0);

  std::deque<std::unique_ptr<Proc>> OptionWorkers;

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

  std::vector<std::thread> PendingReply;

  void onEvalDiagnostic(const ipc::Diagnostics &);

  void onEvalFinished(const ipc::WorkerMessage &);

  template <class Arg, class Resp>
  auto askWorkers(const std::deque<std::unique_ptr<Server::Proc>> &Workers,
                  llvm::StringRef IPCMethod, const Arg &Params,
                  unsigned Timeout) {
    // For all active workers, send the completion request
    std::counting_semaphore Sema(0);
    auto ListStoreOptional = std::make_shared<std::vector<std::optional<Resp>>>(
        Workers.size(), std::nullopt);
    auto ListStoreLock = std::make_shared<std::mutex>();

    size_t I = 0;
    for (const auto &Worker : Workers) {
      auto Request = mkOutMethod<Arg, Resp>(IPCMethod, Worker->OutPort.get());
      Request(Params, [I, ListStoreOptional, ListStoreLock,
                       &Sema](llvm::Expected<Resp> Result) {
        // The worker answered our request, fill the completion lists then.
        Sema.release();
        if (Result) {
          std::lock_guard Guard(*ListStoreLock);
          (*ListStoreOptional)[I] = Result.get();
        } else {
          lspserver::vlog("worker {0} reported error: {1}", I,
                          Result.takeError());
        }
      });
      I++;
    }

    std::future<void> F = std::async([&Sema, Size = I]() {
      for (auto I = 0; I < Size; I++)
        Sema.acquire();
    });

    if (WaitWorker)
      F.wait();
    else
      F.wait_for(std::chrono::microseconds(Timeout));

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

  /// The AttrSet having options, we use this for any nixpkgs options.
  /// nixpkgs basically defined options under "options" attrpath
  /// we can use this for completion (to support ALL option system)
  nix::Value *OptionAttrSet;
  std::unique_ptr<IValueEvalSession> OptionIES;

public:
  Server(std::unique_ptr<lspserver::InboundPort> In,
         std::unique_ptr<lspserver::OutboundPort> Out, int WaitWorker = 0);

  ~Server() override {
    for (auto &T : PendingReply) {
      T.join();
    }
    auto EvalWorkerSize = Workers.size();
    // Ensure that all workers are finished eval, or being killed
    for (auto I = 0; I < EvalWorkerSize; I++) {
      EvalDoneSmp.acquire();
    }
    for (auto &Worker : Workers) {
      Worker.reset();
    }
  }

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

  void evalInstallable(lspserver::PathRef File, int Depth);

  void forkWorker(llvm::unique_function<void()> WorkerAction,
                  std::deque<std::unique_ptr<Proc>> &WorkerPool);

  void forkOptionWorker() {
    forkWorker(
        [this]() {
          switchToOptionProvider();
          Registry.addMethod("nixd/ipc/textDocument/completion/options", this,
                             &Server::onOptionCompletion);
          for (auto &W : OptionWorkers) {
            W->Pid.release();
          }
        },
        OptionWorkers);
    if (OptionWorkers.size() > 1)
      OptionWorkers.pop_front();
  }

  void initWorker();

  void switchToOptionProvider();

  void updateWorkspaceVersion(lspserver::PathRef File);

  void switchToEvaluator(lspserver::PathRef File);

  void updateConfig(configuration::TopLevel &&NewConfig);

  void fetchConfig();

  static llvm::Expected<configuration::TopLevel>
  parseConfig(llvm::StringRef JSON);

  /// Try to update the server config from json encoded file \p File
  /// Won't touch config field if exceptions encountered
  void readJSONConfig(lspserver::PathRef File = ".nixd.json") noexcept;

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

  void onDecalration(const lspserver::TextDocumentPositionParams &,
                     lspserver::Callback<llvm::json::Value>);

  void onDefinition(const lspserver::TextDocumentPositionParams &,
                    lspserver::Callback<llvm::json::Value>);

  void onHover(const lspserver::TextDocumentPositionParams &,
               lspserver::Callback<lspserver::Hover>);

  void onCompletion(const lspserver::CompletionParams &,
                    lspserver::Callback<lspserver::CompletionList>);

  void onFormat(const lspserver::DocumentFormattingParams &,
                lspserver::Callback<std::vector<lspserver::TextEdit>>);

  // Worker

  void onOptionDeclaration(const ipc::AttrPathParams &,
                           lspserver::Callback<lspserver::Location>);

  void onEvalDefinition(const lspserver::TextDocumentPositionParams &,
                        lspserver::Callback<lspserver::Location>);

  void onEvalHover(const lspserver::TextDocumentPositionParams &,
                   lspserver::Callback<llvm::json::Value>);

  void onEvalCompletion(const lspserver::CompletionParams &,
                        lspserver::Callback<llvm::json::Value>);

  void onOptionCompletion(const ipc::AttrPathParams &,
                          lspserver::Callback<llvm::json::Value>);

  //---------------------------------------------------------------------------/
  // Workspace Features

  void onWorkspaceDidChangeConfiguration(
      const lspserver::DidChangeConfigurationParams &) {
    fetchConfig();
  }
};

}; // namespace nixd
