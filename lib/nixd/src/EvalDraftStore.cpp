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
    llvm::unique_function<void(CallbackArg)> Finish, bool AcceptOutdated) {
  {
    std::lock_guard<std::mutex> Guard(Mutex);
    // If we have evaluated before, and that version is not oudated, then return
    // directly.
    if (PreviousResult != nullptr && !IsOutdated) {
      Finish({std::vector<std::exception_ptr>{}, PreviousResult});
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

    // RAII helper object that ensure 'Finish' will be called only once.
    struct CallbackOnceRAII {
      std::vector<std::exception_ptr> Exceptions;
      std::shared_ptr<EvalResult> Result;
      llvm::unique_function<void(CallbackArg)> Finish;
      ~CallbackOnceRAII() { Finish({Exceptions, Result}); }
    } CBRAII;

    CBRAII.Finish = std::move(Finish);

    auto AllCatch = [&]() {
      decltype(EvalResult::EvalASTForest) Forest;

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
        } catch (...) {
          // Catch all exceptions while parsing & evaluation on single file.
          CBRAII.Exceptions.emplace_back(std::current_exception());
        }
      }

      // Installable parsing must do AFTER AST injection.
      auto IValue = nix::InstallableValue::require(
          Cmd.parseInstallable(Cmd.getStore(), Installable));

      // Evaluation goes.
      IValue->toValue(*State);
      CBRAII.Result = std::make_shared<EvalResult>(State, Forest);
      {
        std::lock_guard<std::mutex> Guard(Mutex);
        PreviousResult = CBRAII.Result;
      }
    };

    try {
      AllCatch();
    } catch (...) {
      CBRAII.Exceptions.emplace_back(std::current_exception());
      {
        std::lock_guard<std::mutex> Guard(Mutex);
        if (IsOutdated && AcceptOutdated && PreviousResult != nullptr)
          CBRAII.Result = PreviousResult;
      }
    }
  };

  boost::asio::post(Pool, std::move(Job));
}

} // namespace nixd
