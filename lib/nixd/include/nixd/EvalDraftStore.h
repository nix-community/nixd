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

struct EvalResult {
  nix::ref<nix::EvalState> State;
  std::map<std::string, nix::ref<EvalAST>> EvalASTForest;
  EvalResult(nix::ref<nix::EvalState> State,
             std::map<std::string, nix::ref<EvalAST>> EvalASTForest)
      : State(State), EvalASTForest(std::move(EvalASTForest)) {}
};

/// Logical result per-evaluation
/// Contains nullable EvalResult, and stored exceptions/errors
struct EvalLogicalResult {

  struct InjectionErrorInfo {
    /// Holds the lifetime of associated error.
    std::exception_ptr Ptr;

    /// Which active file caused the error?
    std::string ActiveFile;

    /// Draft version of the corresponding file
    std::string Version;
  };

  /// Maps exceptions -> file for parsing errors
  ///
  /// Injection errors is collected while we do AST injection. Nix is
  /// required to perform parsing and basic evaluation on such single files.
  ///
  /// Injection errors are ignored because opened textDocument might be in
  /// complete, and we don't want to fail early for evaluation. There may be
  /// more than one exceptions on a file.
  std::map<nix::BaseError *, InjectionErrorInfo> InjectionErrors;

  /// Evaluation error, on evaluation task.
  std::exception_ptr EvalError;

  std::shared_ptr<EvalResult> Result;
};

class EvalDraftStore : public lspserver::DraftStore {

public:
  using CallbackArg = EvalLogicalResult;

  void withEvaluation(boost::asio::thread_pool &Pool,
                      const nix::Strings &CommandLine,
                      const std::string &Installable,
                      llvm::unique_function<void(CallbackArg)> Finish);
};
} // namespace nixd
