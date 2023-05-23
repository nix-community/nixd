#pragma once

#include "nixd/AST.h"

#include "lspserver/DraftStore.h"
#include "nixd/CallbackExpr.h"

#include <exception>
#include <map>
#include <mutex>
#include <variant>

namespace nixd {

class EvalDraftStore : public lspserver::DraftStore {

public:
  struct EvaluationResult {
    nix::ref<nix::EvalState> State;
    std::map<std::string, nix::ref<EvalAST>> EvalASTForest;
    EvaluationResult(nix::ref<nix::EvalState> State,
                     std::map<std::string, nix::ref<EvalAST>> EvalASTForest)
        : State(State), EvalASTForest(std::move(EvalASTForest)) {}
  };

private:
  /// Cached evaluation result
  std::mutex Mutex;
  std::shared_ptr<EvaluationResult> PreviousResult;

public:
  void withEvaluation(
      boost::asio::thread_pool &Pool, const nix::Strings &CommandLine,
      const std::string &Installable,
      std::function<
          void(std::variant<std::exception *, nix::ref<EvaluationResult>>)>
          Finish);
};
} // namespace nixd
