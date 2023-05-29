#pragma once

#include "nixd/AST.h"
#include "nixd/CallbackExpr.h"

#include "lspserver/DraftStore.h"

#include <llvm/ADT/FunctionExtras.h>

#include <exception>
#include <map>
#include <mutex>
#include <variant>

namespace nixd {

class EvalDraftStore : public lspserver::DraftStore {

public:
  struct EvalResult {
    nix::ref<nix::EvalState> State;
    std::map<std::string, nix::ref<EvalAST>> EvalASTForest;
    EvalResult(nix::ref<nix::EvalState> State,
               std::map<std::string, nix::ref<EvalAST>> EvalASTForest)
        : State(State), EvalASTForest(std::move(EvalASTForest)) {}
  };

  using CallbackArg =
      std::tuple<std::vector<std::exception_ptr>, std::shared_ptr<EvalResult>>;

private:
  /// Cached evaluation result
  std::mutex Mutex;
  std::shared_ptr<EvalResult> PreviousResult;
  bool IsOutdated = true;

public:
  void withEvaluation(boost::asio::thread_pool &Pool,
                      const nix::Strings &CommandLine,
                      const std::string &Installable,
                      llvm::unique_function<void(CallbackArg)> Finish,
                      bool AcceptOutdated = true);
  std::string addDraft(lspserver::PathRef File, llvm::StringRef Version,
                       llvm::StringRef Contents) {
    {
      std::lock_guard<std::mutex> Guard(Mutex);
      IsOutdated = true;
    }
    return lspserver::DraftStore::addDraft(File, Version, Contents);
  }
};
} // namespace nixd
