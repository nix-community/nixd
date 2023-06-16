#include "nixd/EvalDraftStore.h"

#include "canon-path.hh"
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
      auto SourcePath = nix::CanonPath(AFPath.string());
      auto BasePath = nix::CanonPath(AFPath.remove_filename().string());
      EAF.insert(
          {ActiveFile, nix::make_ref<EvalAST>(*Draft.Contents, SourcePath,
                                              BasePath, *State)});
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
