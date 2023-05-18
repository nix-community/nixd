#pragma once

#include "CallbackExpr.h"
#include "Scheduler.h"

#include "lspserver/Path.h"
#include "lspserver/Protocol.h"

#include <llvm/ADT/FunctionExtras.h>
#include <llvm/ADT/StringRef.h>

#include <nix/error.hh>
#include <nix/installable-value.hh>
#include <nix/installables.hh>
#include <nix/nixexpr.hh>

namespace nixd {

class EvaluationTask : public Task {

public:
  using CallbackAction = llvm::unique_function<void(std::exception *)>;

private:
  CallbackAction Action;

  nix::ref<nix::InstallableValue> IValue;

  nix::ref<nix::EvalState> State;

public:
  EvaluationTask(std::string Name, CallbackAction Action,
                 nix::ref<nix::InstallableValue> IValue,
                 nix::ref<nix::EvalState> State)
      : Task(std::move(Name)), Action(std::move(Action)), IValue(IValue),
        State(State) {}

  void run() noexcept override;
};

/// A Nix language AST wrapper that support language features for LSP.
class NixAST {
#ifndef NDEBUG
  bool Evaluated = false;
#endif
  nix::Expr *Root;
  std::map<const nix::Expr *, const nix::Value *> ValueMap;
  std::map<const nix::Expr *, const nix::Env *> EnvMap;

public:
  /// Inject myself into nix cache.
  void injectAST(nix::EvalState &State, lspserver::PathRef Path);

  /// From fresh-parsed nix::Expr *AST root, e.g. State->parseExprFromString
  NixAST(ASTContext &Cxt, nix::Expr *Root);

  /// Get the evaluation result (fixed point) of the expression.
  const nix::Value *getValue(nix::Expr *Expr) {
    assert(Evaluated && "must be called after evaluation!");
    return ValueMap[Expr];
  }

  /// Get the corresponding 'Env' while evaluating the expression.
  /// nix 'Env's contains dynamic variable name bindings at evaluation, might be
  /// used for completion.
  const nix::Env *getEnv(nix::Expr *Expr) {
    assert(Evaluated && "must be called after evaluation!");
    return EnvMap[Expr];
  }

  /// Lookup an AST node located at the position.
  /// Runs in O(n) (inefficient!)
  nix::Expr *lookupPosition(const nix::EvalState &State, lspserver::Position);

  nix::Expr *root() { return Root; }

  /// Ensure the evaluation task has been done, and invokes callback with `this`
  /// pointer. Some features, like `Env`s, `Value`s, are only available after
  /// evaluation. Evaluation errors shall be passed into the first argument.
  void withEvaluation(
      Scheduler &Scheduler, std::string Name,
      nix::ref<nix::InstallableValue> IValue, nix::ref<nix::EvalState> State,
      llvm::unique_function<void(std::exception *, NixAST *)> Result);
};

/// Parsing ASTs on each document version, providing ASTs & evaluation analysis
/// on requests
class ASTManager {

public:
  /// Parse the AST, any parsing errors will raise an exception.
  /// \p BasicEval indicates that whether to provide an standalone evaluation.
  /// (Nix files are syntax units, and should be evaluated correctly though)
  void parseAST(lspserver::PathRef File, std::string Contents,
                llvm::StringRef Version, bool BasicEval = true);

  /// Get the AST of the file. If there are some errors occurred before, returns
  /// nullptr.
  NixAST *getAST(lspserver::PathRef File, llvm::StringRef Version);
};

} // namespace nixd
