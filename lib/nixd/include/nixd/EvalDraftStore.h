#pragma once

#include "nixd/AST.h"
#include "nixd/CallbackExpr.h"

#include "lspserver/DraftStore.h"

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

  void eval(const std::string &Installable) const {
    auto IValue = nix::InstallableValue::require(
        IVC->parseInstallable(IVC->getStore(), Installable));
    IValue->toValue(*getState());
  }
};

struct IValueEvalResult {
  EvalASTForest Forest;
  std::unique_ptr<IValueEvalSession> Session;

  static const IValueEvalResult *
  search(const std::string &Path,
         const std::vector<const IValueEvalResult *> &Order) {
    for (const auto &Result : Order) {
      try {
        Result->Forest.at(Path);
        return Result;
      } catch (...) {
      }
    }
    return nullptr;
  }
};

/// Ownes `EvalASTForest`s, mark it is evaluated or not, clients query on this
/// cache to find a suitable AST Tree
struct ForestCache {

  IValueEvalResult EvaluatedResult;

  IValueEvalResult NonEmptyResult;

  enum class ASTPreference { Evaluated, NonEmpty };

  [[nodiscard]] const IValueEvalResult *
  searchAST(const std::string &Path, ASTPreference Preference) const {
    switch (Preference) {
    case ASTPreference::Evaluated:
      return IValueEvalResult::search(Path,
                                      {&EvaluatedResult, &NonEmptyResult});
    case ASTPreference::NonEmpty:
      return IValueEvalResult::search(Path,
                                      {&NonEmptyResult, &EvaluatedResult});
    }
  };
};
} // namespace nixd
