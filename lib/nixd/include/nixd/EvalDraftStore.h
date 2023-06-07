#pragma once

#include "nixd/AST.h"
#include "nixd/CallbackExpr.h"
#include "nixd/nix/EvalState.h"

#include "lspserver/DraftStore.h"
#include "lspserver/Logger.h"

#include <nix/command-installable-value.hh>
#include <nix/installable-value.hh>

#include <llvm/ADT/FunctionExtras.h>

#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <variant>

namespace nixd {

using EvalASTForest = std::map<std::string, nix::ref<EvalAST>>;

struct InjectionErrorInfo {
  /// Holds the lifetime of associated error.
  std::exception_ptr Ptr;

  /// Which active file caused the error?
  std::string ActiveFile;

  /// Draft version of the corresponding file
  std::string Version;
};

struct InjectionLogicalResult {
  /// Maps exceptions -> file for parsing errors
  ///
  /// Injection errors is collected while we do AST injection. Nix is
  /// required to perform parsing and basic evaluation on such single files.
  ///
  /// Injection errors are ignored because opened textDocument might be in
  /// complete, and we don't want to fail early for evaluation. There may be
  /// more than one exceptions on a file.
  std::map<nix::BaseError *, InjectionErrorInfo> InjectionErrors;

  /// Holds EvalASTs
  EvalASTForest Forest;
};

class EvalDraftStore : public lspserver::DraftStore {

public:
  /// Inject file in this draft store, and return logical results
  InjectionLogicalResult
  injectFiles(const nix::ref<nix::EvalState> &State) noexcept;
};

/// Installable evaluation session. Owns the eval state & command & store in
/// each evaluation task.
/// `InstallableValue`s must be evaluated AFTER AST injection. Or nix will try
/// to access real filesystems. So the typical procedure of each evaluation is:
/// 1. Construct a "InstallableValueCommand"
/// 2. Parse args, given by our user.
/// 3. Get a "State" from this command
/// 4. Tree injection
/// 5. Eval
struct IValueEvalSession {
  std::unique_ptr<nix::InstallableValueCommand> IVC;

  class SimpleCmd : public nix::InstallableValueCommand {
    void run(nix::ref<nix::Store>, nix::ref<nix::InstallableValue>) override {}
  };

  IValueEvalSession() : IVC(std::make_unique<SimpleCmd>()){};
  IValueEvalSession(decltype(IVC) IVC) : IVC(std::move(IVC)) {}

  /// Parse the command line, exceptions (e.g. UsageError) may thrown by nix if
  /// something goes wrong.
  void parseArgs(const nix::Strings &CL) const { IVC->parseCmdline(CL); }

  /// Get the eval state held. Clients my use this stuff to inject trees
  [[nodiscard]] nix::ref<nix::EvalState> getState() const {
    return IVC->getEvalState();
  };

  nix::Value *eval(const std::string &Installable, int Depth = 0) const {
    lspserver::log("evaluation on installable {0}, requested depth: {1}",
                   Installable, Depth);
    auto IValue = nix::InstallableValue::require(
        IVC->parseInstallable(IVC->getStore(), Installable));
    auto [Value, _] = IValue->toValue(*getState());
    nix::forceValueDepth(*IVC->getEvalState(), *Value, Depth);
    return Value;
  }
};

struct IValueEvalResult {
  EvalASTForest Forest;
  std::unique_ptr<IValueEvalSession> Session;

  IValueEvalResult(decltype(Forest) Forest, decltype(Session) Session)
      : Forest(std::move(Forest)), Session(std::move(Session)) {}
};

} // namespace nixd
