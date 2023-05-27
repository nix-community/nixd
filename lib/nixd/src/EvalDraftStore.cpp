#include "nixd/EvalDraftStore.h"

#include "lspserver/Logger.h"

#include <nix/command-installable-value.hh>
#include <nix/eval.hh>
#include <nix/installable-value.hh>
#include <nix/nixexpr.hh>

#include <filesystem>
#include <mutex>

namespace nixd {

void EvalDraftStore::withEvaluation(
    boost::asio::thread_pool &Pool, const nix::Strings &CommandLine,
    const std::string &Installable,
    llvm::unique_function<
        void(std::variant<std::exception *, nix::ref<EvaluationResult>>)>
        Finish,
    bool AcceptOutdated) {
  {
    std::lock_guard<std::mutex> Guard(Mutex);
    // If we have evaluated before, and that version is not oudated, then return
    // directly.
    if (PreviousResult != nullptr && !IsOutdated) {
      Finish(nix::ref(PreviousResult));
      return;
    }
  }

  // Otherwise, we must parse all files, rewrite trees, and schedule evaluation.

  class MyCmd : public nix::InstallableValueCommand {
    void run(nix::ref<nix::Store>, nix::ref<nix::InstallableValue>) override {}
  } Cmd;

  Cmd.parseCmdline(CommandLine);

  nix::ref<nix::EvalState> State = Cmd.getEvalState();

  auto Job = [=, Finish = std::move(Finish), this]() mutable noexcept {
    // Catch ALL exceptions in this block
    auto AllCatch = [&]() {
      decltype(EvaluationResult::EvalASTForest) Forest;

      // First, parsing all active files and rewrite their AST
      auto ActiveFiles = getActiveFiles();
      for (const auto &ActiveFile : ActiveFiles) {
        // Safely unwrap optional because they are stored active files.
        auto DraftContents = *getDraft(ActiveFile).value().Contents;

        try {
          std::filesystem::path Path = ActiveFile;
          auto *FileAST =
              State->parseExprFromString(DraftContents, Path.remove_filename());
          Forest.insert({ActiveFile, nix::make_ref<EvalAST>(FileAST)});
          Forest.at(ActiveFile)->preparePositionLookup(*State);
          Forest.at(ActiveFile)->injectAST(*State, ActiveFile);
        } catch (const nix::ParseError &) {
          // Ignore parsing errors, because workspace file might be incomplete.
        }
      }

      // Evaluation goes.
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
    };

    try {
      AllCatch();
    } catch (std::exception &Except) {
      {
        std::lock_guard<std::mutex> Guard(Mutex);
        if (IsOutdated && AcceptOutdated && PreviousResult != nullptr) {
          // Just use previous result, do not report diagnostic
          // Useful for completions, because the tree might be imcomplete.
          Finish(nix::ref(PreviousResult));
        }
      }
      Finish(&Except);
      return;
    } catch (...) {
      lspserver::log("Evaluation task encountered an unknown exception!");
      return;
    }
  };

  boost::asio::post(Pool, std::move(Job));
}

} // namespace nixd
