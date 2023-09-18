#pragma once

#include "EvalDraftStore.h"

#include "nixd/Parser/Require.h"
#include "nixd/Server/ASTManager.h"
#include "nixd/Server/ConfigSerialization.h"
#include "nixd/Server/IPCSerialization.h"
#include "nixd/Support/Diagnostic.h"
#include "nixd/Support/JSONSerialization.h"
#include "nixd/Support/ReplyRAII.h"

#include "lspserver/Connection.h"
#include "lspserver/DraftStore.h"
#include "lspserver/Function.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Logger.h"
#include "lspserver/Path.h"
#include "lspserver/Protocol.h"
#include "lspserver/SourceCode.h"

#include <llvm/ADT/FunctionExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>

#include <boost/asio/thread_pool.hpp>
#include <boost/thread.hpp>

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <semaphore>
#include <shared_mutex>
#include <stdexcept>
#include <thread>

#include <pthread.h>

namespace nixd {

/// The server instance, nix-related language features goes here
class Controller : public lspserver::LSPServer {
public:
  using WorkspaceVersionTy = ipc::WorkspaceVersionTy;

  struct Proc {
    std::unique_ptr<nix::Pipe> ToPipe;
    std::unique_ptr<nix::Pipe> FromPipe;
    std::unique_ptr<lspserver::OutboundPort> OutPort;
    std::unique_ptr<llvm::raw_ostream> OwnedStream;

    nix::Pid Pid;
    std::thread InputDispatcher;

    std::counting_semaphore<> &Smp;

    bool WaitWorker;

    [[nodiscard]] nix::AutoCloseFD to() const {
      return ToPipe->writeSide.get();
    };
    [[nodiscard]] nix::AutoCloseFD from() const {
      return FromPipe->readSide.get();
    };

    ~Proc() {
      if (WaitWorker) {
        auto Th = InputDispatcher.native_handle();
        pthread_cancel(Th);
        InputDispatcher.join();
      } else
        InputDispatcher.detach();
    }
  };

  template <class ReplyTy>
  void withParseAST(
      ReplyRAII<ReplyTy> &&RR, const std::string &Path,
      llvm::unique_function<void(ReplyRAII<ReplyTy> &&RR, const ParseAST &AST,
                                 ASTManager::VersionTy Version)>
          Action) noexcept {
    if (auto Draft = DraftMgr.getDraft(Path)) {
      auto Version = EvalDraftStore::decodeVersion(Draft->Version).value_or(0);
      ASTMgr.withAST(
          Path, Version,
          [RR = std::move(RR), Action = std::move(Action)](
              const ParseAST &AST, ASTManager::VersionTy &Version) mutable {
            Action(std::move(RR), AST, Version);
          });
    } else {
      RR.Response = lspserver::error("no draft available (removed before?)");
    }
  }

private:
  bool WaitWorker = false;

  using WorkerContainerLock = std::shared_mutex;
  using WorkerContainer = std::deque<std::unique_ptr<Proc>>;

  using WC = std::tuple<const WorkerContainer &, std::shared_mutex &, size_t>;

  WorkspaceVersionTy WorkspaceVersion = 1;

  WorkerContainerLock EvalWorkerLock;
  WorkerContainer EvalWorkers; // GUARDED_BY(EvalWorkerLock)

  // Used for lit tests, ensure that workers have finished their job.
  std::counting_semaphore<> FinishSmp = std::counting_semaphore(0);

  WorkerContainerLock OptionWorkerLock;
  WorkerContainer OptionWorkers; // GUARDED_BY(OptionWorkerLock)

  EvalDraftStore DraftMgr;

  ASTManager ASTMgr;

  lspserver::ClientCapabilities ClientCaps;

  std::mutex ConfigLock;

  configuration::TopLevel JSONConfig;
  configuration::TopLevel Config; // GUARDED_BY(ConfigLock)

  std::shared_ptr<const std::string> getDraft(lspserver::PathRef File) const;

  void addDocument(lspserver::PathRef File, llvm::StringRef Contents,
                   llvm::StringRef Version);

  void removeDocument(lspserver::PathRef File) { DraftMgr.removeDraft(File); }

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

  boost::asio::thread_pool Pool;

  // IPC Utils

  template <class Resp, class Arg>
  auto askWorkers(const WorkerContainer &Workers, std::shared_mutex &WorkerLock,
                  llvm::StringRef IPCMethod, const Arg &Params,
                  unsigned Timeout);

  template <class Resp, class Arg>
  auto askWC(llvm::StringRef IPCMethod, const Arg &Params, WC CL)
      -> std::vector<Resp> {
    auto &[W, L, T] = CL;
    return askWorkers<Resp>(W, L, IPCMethod, Params, T);
  }

  template <class Resp, class Arg, class... A>
  auto askWC(llvm::StringRef IPCMethod, const Arg &Params, WC CL, A... Rest)
      -> std::vector<Resp> {
    auto Vec = askWC<Resp>(IPCMethod, Params, CL);
    if (Vec.empty())
      return askWC<Resp>(IPCMethod, Params, Rest...);
    return Vec;
  }

  void syncDrafts(Proc &);

public:
  Controller(std::unique_ptr<lspserver::InboundPort> In,
             std::unique_ptr<lspserver::OutboundPort> Out, int WaitWorker = 0);

  ~Controller() override {
    if (WaitWorker) {
      Pool.join();
      std::lock_guard Guard(EvalWorkerLock);
      // Ensure that all workers are finished eval, or being killed
      for (size_t I = 0; I < EvalWorkers.size(); I++) {
        FinishSmp.acquire();
      }
    }
    for (auto &Worker : EvalWorkers) {
      Worker.reset();
    }
  }

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

  void clearDiagnostic(lspserver::PathRef Path);

  void clearDiagnostic(const lspserver::URIForFile &FileUri);

  void onDecalration(const lspserver::TextDocumentPositionParams &,
                     lspserver::Callback<llvm::json::Value>);

  void onDefinition(const lspserver::TextDocumentPositionParams &,
                    lspserver::Callback<llvm::json::Value>);

  void
  onDocumentLink(const lspserver::DocumentLinkParams &,
                 lspserver::Callback<std::vector<lspserver::DocumentLink>>);

  void
  onDocumentSymbol(const lspserver::DocumentSymbolParams &Params,
                   lspserver::Callback<std::vector<lspserver::DocumentSymbol>>);

  void onHover(const lspserver::TextDocumentPositionParams &,
               lspserver::Callback<lspserver::Hover>);

  void onCompletion(const lspserver::CompletionParams &,
                    lspserver::Callback<llvm::json::Value>);

  void onFormat(const lspserver::DocumentFormattingParams &,
                lspserver::Callback<std::vector<lspserver::TextEdit>>);

  void onRename(const lspserver::RenameParams &,
                lspserver::Callback<lspserver::WorkspaceEdit>);

  void onPrepareRename(const lspserver::TextDocumentPositionParams &,
                       lspserver::Callback<llvm::json::Value>);

  //---------------------------------------------------------------------------/
  // Workspace Features

  void updateConfig(configuration::TopLevel &&NewConfig);

  void fetchConfig();

  static llvm::Expected<configuration::TopLevel>
  parseConfig(llvm::StringRef JSON);

  /// Try to update the server config from json encoded file \p File
  /// Won't touch config field if exceptions encountered
  void readJSONConfig(lspserver::PathRef File = ".nixd.json") noexcept;

  void onWorkspaceDidChangeConfiguration(
      const lspserver::DidChangeConfigurationParams &) {
    fetchConfig();
  }

  //---------------------------------------------------------------------------/
  // Interprocess

  // Controller

  /// Returns non-null worker process handle.
  std::unique_ptr<Proc> forkWorker(llvm::unique_function<void()> WorkerAction);

  std::unique_ptr<Proc> selfExec(char **Argv) {
    return forkWorker([Argv]() {
      if (auto Name = nix::getSelfExe()) {
        execv(Name->c_str(), Argv);
      } else {
        throw std::runtime_error("nix::getSelfExe() returned nullopt");
      }
    });
  }

  std::unique_ptr<Proc> createEvalWorker();

  std::unique_ptr<Proc> createOptionWorker();

  bool createEnqueueEvalWorker();

  bool createEnqueueOptionWorker();

  static bool enqueueWorker(WorkerContainerLock &Lock,
                            WorkerContainer &Container,
                            std::unique_ptr<Proc> Worker, size_t Size);

  void onEvalDiagnostic(const ipc::Diagnostics &);

  void onFinished(const ipc::WorkerMessage &);

  // Worker::Nix::Option

  void forkOptionWorker();

  void switchToOptionProvider();

  void onOptionDeclaration(const ipc::AttrPathParams &,
                           lspserver::Callback<lspserver::Location>);

  void onOptionCompletion(const ipc::AttrPathParams &,
                          lspserver::Callback<llvm::json::Value>);
};

template <class Resp, class Arg>
auto Controller::askWorkers(
    const std::deque<std::unique_ptr<Controller::Proc>> &Workers,
    std::shared_mutex &WorkerLock, llvm::StringRef IPCMethod, const Arg &Params,
    unsigned Timeout) {
  // For all active workers, send the completion request
  auto ListStoreOptional = std::make_shared<std::vector<std::optional<Resp>>>(
      Workers.size(), std::nullopt);
  auto Sema = std::make_shared<std::counting_semaphore<>>(0);
  auto ListStoreLock = std::make_shared<std::mutex>();

  size_t I = 0;
  {
    std::shared_lock RLock(WorkerLock);
    for (const auto &Worker : Workers) {
      auto Request = mkOutMethod<Arg, Resp>(IPCMethod, Worker->OutPort.get());
      Request(Params, [I, ListStoreOptional, ListStoreLock,
                       Sema](llvm::Expected<Resp> Result) {
        // The worker answered our request, fill the completion lists then.
        if (Result) {
          std::lock_guard Guard(*ListStoreLock);
          (*ListStoreOptional)[I] = Result.get();
        } else {
          lspserver::vlog("worker {0} reported error: {1}", I,
                          Result.takeError());
        }
        Sema->release();
      });
      I++;
    }
  }

  std::future<void> F =
      std::async([Sema, Size = I, Timeout, WaitWorker = this->WaitWorker]() {
        for (size_t I = 0; I < Size; I++) {
          if (WaitWorker)
            Sema->acquire();
          else
            Sema->try_acquire_for(std::chrono::microseconds(Timeout / Size));
        }
      });

  F.wait();

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

}; // namespace nixd
