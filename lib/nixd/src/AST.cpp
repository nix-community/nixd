#include "nixd/AST.h"
#include "nixd/CallbackExpr.h"

#include "lspserver/Logger.h"

#include <exception>
#include <memory>

namespace nixd {

NixAST::NixAST(ASTContext &Cxt, nix::Expr *Root) {
  auto EvalCallback = [this](const nix::Expr *Expr, const nix::EvalState &,
                             const nix::Env &ExprEnv,
                             const nix::Value &ExprValue) {
    ValueMap[Expr] = ExprValue;
    EnvMap[Expr] = ExprEnv;
  };
  this->Root = rewriteCallback(Cxt, EvalCallback, Root);
}

void NixAST::injectAST(nix::EvalState &State, lspserver::PathRef Path) {
  nix::Value DummyValue{};
  State.cacheFile(Path.str(), Path.str(), Root, DummyValue);
}

void NixAST::withEvaluation(
    boost::asio::thread_pool &Pool, nix::ref<nix::InstallableValue> IValue,
    nix::ref<nix::EvalState> State,
    llvm::unique_function<void(std::exception *, NixAST *)> Result) {
  auto Action = [this,
                 Result = std::move(Result)](std::exception *Except) mutable {
    if (Except == nullptr) {
#ifndef NDEBUG
      Evaluated = true;
#endif
      Result(nullptr, this);
    } else {
      Result(Except, nullptr);
    }
  };
  boost::asio::post(Pool,
                    [IValue, Action = std::move(Action), State]() mutable {
                      try {
                        IValue->toValue(*State);
                        Action(nullptr);
                      } catch (std::exception &Except) {
                        lspserver::log(Except.what());
                        Action(&Except);
                      }
                    });
  Pool.join();
}

} // namespace nixd
