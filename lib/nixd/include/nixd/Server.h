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

#include <boost/thread.hpp>

#include <chrono>
#include <cstdint>
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
  bool WaitWorker = false;

  ServerRole Role = ServerRole::Controller;

  WorkspaceVersionTy WorkspaceVersion = 1;

  std::shared_mutex EvalWorkerLock;
  std::deque<std::unique_ptr<Proc>> EvalWorkers; // GUARDED_BY(EvalWorkerLock)

  // Evaluator notify us "evaluation done" by incresing this semaphore.
  std::counting_semaphore<> EvalDoneSmp = std::counting_semaphore(0);

  std::shared_mutex OptionWorkerLock;
  std::deque<std::unique_ptr<Proc>>
      OptionWorkers; // GUARDED_BY(OptionWorkerLock)

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

  boost::asio::thread_pool Pool;

  //---------------------------------------------------------------------------/
  // Worker members

  // Worker::Option

  /// The AttrSet having options, we use this for any nixpkgs options.
  /// nixpkgs basically defined options under "options" attrpath
  /// we can use this for completion (to support ALL option system)
  nix::Value *OptionAttrSet;
  std::unique_ptr<IValueEvalSession> OptionIES;

  // Worker::Eval

  llvm::unique_function<void(const ipc::Diagnostics &)> EvalDiagnostic;

  std::unique_ptr<IValueEvalResult> IER;

public:
  Server(std::unique_ptr<lspserver::InboundPort> In,
         std::unique_ptr<lspserver::OutboundPort> Out, int WaitWorker = 0);

  ~Server() override {
    if (WaitWorker) {
      Pool.join();
      std::lock_guard Guard(EvalWorkerLock);
      auto EvalWorkerSize = EvalWorkers.size();
      // Ensure that all workers are finished eval, or being killed
      for (decltype(EvalWorkerSize) I = 0; I < EvalWorkerSize; I++) {
        EvalDoneSmp.acquire();
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

  void updateWorkspaceVersion(lspserver::PathRef File);

  void onDocumentDidOpen(const lspserver::DidOpenTextDocumentParams &Params);

  void
  onDocumentDidChange(const lspserver::DidChangeTextDocumentParams &Params);

  void onDocumentDidClose(const lspserver::DidCloseTextDocumentParams &Params);

  //---------------------------------------------------------------------------/
  // Language Features

  void clearDiagnostic(lspserver::PathRef Path);

  void clearDiagnostic(const lspserver::URIForFile &FileUri);

  static lspserver::PublishDiagnosticsParams
  diagNixError(lspserver::PathRef Path, const nix::BaseError &NixErr,
               std::optional<int64_t> Version);

  void publishStandaloneDiagnostic(lspserver::URIForFile Uri,
                                   std::string Content,
                                   std::optional<int64_t> LSPVersion);

  void onDecalration(const lspserver::TextDocumentPositionParams &,
                     lspserver::Callback<llvm::json::Value>);

  void onDefinition(const lspserver::TextDocumentPositionParams &,
                    lspserver::Callback<llvm::json::Value>);

  void
  onDocumentLink(const lspserver::DocumentLinkParams &,
                 lspserver::Callback<std::vector<lspserver::DocumentLink>>);

  void onHover(const lspserver::TextDocumentPositionParams &,
               lspserver::Callback<lspserver::Hover>);

  void onCompletion(const lspserver::CompletionParams &,
                    lspserver::Callback<lspserver::CompletionList>);

  void onFormat(const lspserver::DocumentFormattingParams &,
                lspserver::Callback<std::vector<lspserver::TextEdit>>);

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

  void forkWorker(llvm::unique_function<void()> WorkerAction,
                  std::deque<std::unique_ptr<Proc>> &WorkerPool);

  void onEvalDiagnostic(const ipc::Diagnostics &);

  void onEvalFinished(const ipc::WorkerMessage &);

  template <class Arg, class Resp>
  auto askWorkers(const std::deque<std::unique_ptr<Server::Proc>> &Workers,
                  std::shared_mutex &WorkerLock, llvm::StringRef IPCMethod,
                  const Arg &Params, unsigned Timeout);

  // Worker

  void initWorker();

  // Worker::Nix::Option

  void forkOptionWorker();

  void switchToOptionProvider();

  void onOptionDeclaration(const ipc::AttrPathParams &,
                           lspserver::Callback<lspserver::Location>);

  void onOptionCompletion(const ipc::AttrPathParams &,
                          lspserver::Callback<llvm::json::Value>);

  // Worker::Nix::Eval

  void switchToEvaluator(lspserver::PathRef File);

  template <class ReplyTy>
  void withAST(
      const std::string &, ReplyRAII<ReplyTy>,
      llvm::unique_function<void(nix::ref<EvalAST>, ReplyRAII<ReplyTy> &&)>);

  void evalInstallable(lspserver::PathRef File, int Depth);

  void onEvalDefinition(const lspserver::TextDocumentPositionParams &,
                        lspserver::Callback<lspserver::Location>);

  void
  onEvalDocumentLink(const lspserver::TextDocumentIdentifier &,
                     lspserver::Callback<std::vector<lspserver::DocumentLink>>);

  void onEvalHover(const lspserver::TextDocumentPositionParams &,
                   lspserver::Callback<llvm::json::Value>);

  void onEvalCompletion(const lspserver::CompletionParams &,
                        lspserver::Callback<llvm::json::Value>);
};

template <class Arg, class Resp>
auto Server::askWorkers(
    const std::deque<std::unique_ptr<Server::Proc>> &Workers,
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
            Sema->try_acquire_for(std::chrono::microseconds(Timeout));
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

template <class ReplyTy>
void Server::withAST(
    const std::string &RequestedFile, ReplyRAII<ReplyTy> RR,
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

}; // namespace nixd
