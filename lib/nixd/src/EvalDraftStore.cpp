#include "nixd/EvalDraftStore.h"

#include <nix/command-installable-value.hh>
#include <nix/eval.hh>
#include <nix/installable-value.hh>
#include <nix/nixexpr.hh>

namespace nixd {

void EvalDraftStore::withEvaluation(
    boost::asio::thread_pool &Pool, const nix::Strings &CommandLine,
    const std::string &Installable,
    std::function<
        void(std::variant<std::exception *, nix::ref<EvaluationResult>>)>
        Finish) {

  if (PreviousResult != nullptr) {
    Finish(nix::ref(PreviousResult));
    return;
  }

  class MyCmd : public nix::InstallableValueCommand {
    void run(nix::ref<nix::Store>, nix::ref<nix::InstallableValue>) override {}
  } Cmd;

  Cmd.parseCmdline(CommandLine);

  nix::ref<nix::EvalState> State = Cmd.getEvalState();

  auto Job = [=, Finish = std::move(Finish), this]() mutable {
    // First, parsing all active files and rewrite their AST
    decltype(EvaluationResult::EvalASTForest) Forest;

    auto ActiveFiles = getActiveFiles();
    for (const auto &ActiveFile : ActiveFiles) {
      // Safely unwrap optional because they are stored active files.
      auto DraftContents = *getDraft(ActiveFile).value().Contents;

      // TODO: should we canoicalize 'basePath'?
      auto FileAST = State->parseExprFromString(DraftContents, ActiveFile);
      Forest.insert({ActiveFile, nix::make_ref<EvalAST>(FileAST)});
      Forest.at(ActiveFile)->injectAST(*State, ActiveFile);
    }

    // Evaluation goes.
    try {
      // Installable parsing must do AFTER ast injection.
      auto IValue = nix::InstallableValue::require(
          Cmd.parseInstallable(Cmd.getStore(), Installable));
      IValue->toValue(*State);
      auto EvalResult = nix::make_ref<EvaluationResult>(State, Forest);
      Finish(EvalResult);
      {
        std::lock_guard<std::mutex> Guard(Mutex);
        PreviousResult = EvalResult;
      }
      return;
    } catch (std::exception *E) {
      Finish(E);
      return;
    }
  };

  boost::asio::post(Pool, Job);
}

} // namespace nixd
