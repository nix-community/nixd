#include "nixd/EvalDraftStore.h"

#include "lspserver/Logger.h"

#include <nix/command-installable-value.hh>
#include <nix/eval.hh>
#include <nix/installable-value.hh>
#include <nix/nixexpr.hh>

#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>

namespace nixd {

InjectionLogicalResult
EvalDraftStore::injectFiles(const nix::ref<nix::EvalState> &State) noexcept {
  InjectionLogicalResult ILR;
  EvalASTForest &EAF = ILR.Forest;
  auto ActiveFiles = getActiveFiles();
  for (const auto &ActiveFile : ActiveFiles) {
    // Safely unwrap optional because they are stored active files.
    auto Draft = getDraft(ActiveFile).value();
    try {
      std::filesystem::path AFPath = ActiveFile;
      auto *FileAST =
          State->parseExprFromString(*Draft.Contents, AFPath.remove_filename());
      EAF.insert({ActiveFile, nix::make_ref<EvalAST>(FileAST)});
      EAF.at(ActiveFile)->preparePositionLookup(*State);
      EAF.at(ActiveFile)->prepareParentTable();
      EAF.at(ActiveFile)->injectAST(*State, ActiveFile);
    } catch (nix::BaseError &Err) {
      std::exception_ptr Ptr = std::current_exception();
      ILR.InjectionErrors.insert({&Err, {Ptr, ActiveFile, Draft.Version}});
    } catch (...) {
      // Catch all exceptions while parsing & evaluation on single file.
    }
  }
  return ILR;
}
} // namespace nixd
