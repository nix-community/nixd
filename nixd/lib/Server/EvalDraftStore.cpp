#include "nixd/Server/EvalDraftStore.h"

#include "lspserver/Logger.h"

#include <nix/canon-path.hh>
#include <nix/command-installable-value.hh>
#include <nix/error.hh>
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
      auto ParseData = parse(*Draft.Contents, SourcePath, BasePath, *State);
      if (!ParseData->error.empty()) {
        for (const auto &ErrInfo : ParseData->error) {
          auto ParseError = std::make_unique<nix::ParseError>(ErrInfo);
          ILR.InjectionErrors.emplace_back(
              InjectionError{std::move(ParseError), ActiveFile, Draft.Version});
        }
      }
      auto AST = EvalAST::create(std::move(ParseData), *State);
      EAF.insert({ActiveFile, nix::ref<EvalAST>(std::move(AST))});
      EAF.at(ActiveFile)->injectAST(*State, ActiveFile);
    } catch (nix::BaseError &Err) {
      ILR.InjectionErrors.emplace_back(InjectionError{
          std::make_unique<nix::BaseError>(Err), ActiveFile, Draft.Version});
    } catch (...) {
      // Catch all exceptions while parsing & evaluation on single file.
    }
  }
  return ILR;
}
} // namespace nixd
