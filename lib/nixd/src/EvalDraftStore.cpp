#include "nixd/EvalDraftStore.h"

#include "lspserver/Logger.h"

#include <exception>
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
    llvm::unique_function<void(CallbackArg)> Finish) {

  class MyCmd : public nix::InstallableValueCommand {
    void run(nix::ref<nix::Store>, nix::ref<nix::InstallableValue>) override {}
  } Cmd;

  Cmd.parseCmdline(CommandLine);

  nix::ref<nix::EvalState> State = Cmd.getEvalState();

  auto Job = [=, Finish = std::move(Finish), this]() mutable noexcept {
    // Catch ALL exceptions in this block

    // RAII helper object that ensures 'Finish' will be called only once.
    struct CallbackOnceRAII {

      EvalLogicalResult LogicalResult;
      CallbackOnceRAII(llvm::unique_function<void(CallbackArg)> Finish)
          : Finish(std::move(Finish)) {}
      ~CallbackOnceRAII() { Finish(LogicalResult); }

    private:
      llvm::unique_function<void(CallbackArg)> Finish;
    } CBRAII(std::move(Finish));

    auto AllCatch = [&]() {
      decltype(EvalResult::EvalASTForest) Forest;

      // First, parsing all active files and rewrite their AST
      auto ActiveFiles = getActiveFiles();
      for (const auto &ActiveFile : ActiveFiles) {
        // Safely unwrap optional because they are stored active files.
        auto Draft = getDraft(ActiveFile).value();

        try {
          std::filesystem::path Path = ActiveFile;
          auto *FileAST = State->parseExprFromString(*Draft.Contents,
                                                     Path.remove_filename());
          Forest.insert({ActiveFile, nix::make_ref<EvalAST>(FileAST)});
          Forest.at(ActiveFile)->preparePositionLookup(*State);
          Forest.at(ActiveFile)->injectAST(*State, ActiveFile);
        } catch (nix::BaseError &Err) {
          std::exception_ptr Ptr = std::current_exception();
          CBRAII.LogicalResult.InjectionErrors.insert(
              {&Err, {Ptr, ActiveFile, Draft.Version}});
        } catch (...) {
          // Catch all exceptions while parsing & evaluation on single file.
        }
      }

      if (Forest.empty())
        return;

      // Installable parsing must do AFTER AST injection.
      auto IValue = nix::InstallableValue::require(
          Cmd.parseInstallable(Cmd.getStore(), Installable));

      // Evaluation goes.
      IValue->toValue(*State);
      CBRAII.LogicalResult.Result = std::make_shared<EvalResult>(State, Forest);
    };

    try {
      AllCatch();
    } catch (...) {
      CBRAII.LogicalResult.EvalError = std::current_exception();
    }
  };

  Job();
}

} // namespace nixd
