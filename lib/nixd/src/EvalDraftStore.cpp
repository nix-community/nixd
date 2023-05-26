#include "nixd/EvalDraftStore.h"

#include <filesystem>
#include <nix/command-installable-value.hh>
#include <nix/eval.hh>
#include <nix/installable-value.hh>
#include <nix/nixexpr.hh>

#include <mutex>

namespace nixd {

void EvalDraftStore::withEvaluation(
    boost::asio::thread_pool &Pool, const nix::Strings &CommandLine,
    const std::string &Installable,
    llvm::unique_function<
        void(std::variant<const std::exception *, nix::ref<EvaluationResult>>)>
        Finish) {
  {
    std::lock_guard<std::mutex> Guard(Mutex);
    if (PreviousResult != nullptr) {
      Finish(nix::ref(PreviousResult));
      return;
    }
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
      std::filesystem::path Path = ActiveFile;
      try {
        auto *FileAST =
            State->parseExprFromString(DraftContents, Path.remove_filename());
        Forest.insert({ActiveFile, nix::make_ref<EvalAST>(FileAST)});
        Forest.at(ActiveFile)->preparePositionLookup(*State);
        Forest.at(ActiveFile)->injectAST(*State, ActiveFile);
      } catch (const std::exception &Except) {
        Finish(&Except);
        return;
      }
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
    } catch (const std::exception &Except) {
      Finish(&Except);
      return;
    }
  };

  boost::asio::post(Pool, std::move(Job));
}

} // namespace nixd
