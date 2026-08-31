#include "nixd/Controller/Startup.h"
#include "nixd/Support/JSON.h"

#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/MemoryBuffer.h>

#include <cerrno>
#include <system_error>
#include <unistd.h>

using namespace nixd;

namespace {

struct RootCandidate {
  std::filesystem::path path;
  std::string source;
};

StartupSelection fallback(const CommandLineConfiguration &CLI,
                          const std::filesystem::path &LaunchCWD,
                          std::optional<std::string> Warning = std::nullopt) {
  return {
      .baseConfiguration = CLI.baseConfiguration,
      .executionCWD = LaunchCWD,
      .warning = std::move(Warning),
  };
}

std::optional<std::string> unusableReason(const std::filesystem::path &Path) {
  std::error_code EC;
  const std::filesystem::file_status Status = std::filesystem::status(Path, EC);
  if (EC)
    return EC.message();
  if (!std::filesystem::exists(Status))
    return "path does not exist";
  if (!std::filesystem::is_directory(Status))
    return "path is not a directory";
  if (access(Path.c_str(), X_OK) != 0)
    return std::error_code(errno, std::generic_category()).message();
  return std::nullopt;
}

std::string invalidCandidate(llvm::StringRef Source, llvm::StringRef Reason) {
  return llvm::formatv("cannot use {0} as project configuration root: {1}",
                       Source, Reason)
      .str();
}

std::string unusableCandidate(const RootCandidate &Candidate,
                              llvm::StringRef Reason) {
  return llvm::formatv("cannot use {0} as project configuration root {1}: {2}",
                       Candidate.source, Candidate.path.string(), Reason)
      .str();
}

std::string loadFailure(const std::filesystem::path &Path,
                        llvm::StringRef Reason) {
  return llvm::formatv("failed to load project configuration at {0}: {1}",
                       Path.string(), Reason)
      .str();
}

} // namespace

StartupSelection nixd::selectStartup(const CommandLineConfiguration &CLI,
                                     const lspserver::InitializeParams &Params,
                                     const std::filesystem::path &LaunchCWD) {
  if (!CLI.enableProjectConfig || CLI.configSpecified)
    return fallback(CLI, LaunchCWD);

  const std::size_t WorkspaceFolderCount =
      Params.workspaceFoldersRawSize.value_or(
          Params.workspaceFolders ? Params.workspaceFolders->size() : 0);
  if (WorkspaceFolderCount > 1)
    return fallback(
        CLI, LaunchCWD,
        llvm::formatv("cannot load project configuration with multiple "
                      "workspace folders ({0} supplied)",
                      WorkspaceFolderCount)
            .str());

  RootCandidate Candidate;
  if (Params.rootUriError)
    return fallback(CLI, LaunchCWD,
                    invalidCandidate("rootUri", *Params.rootUriError));
  if (Params.rootUri) {
    Candidate = {.path = Params.rootUri->file().str(), .source = "rootUri"};
  } else {
    if (Params.rootPathError)
      return fallback(CLI, LaunchCWD,
                      invalidCandidate("rootPath", *Params.rootPathError));
    if (Params.rootPath && !Params.rootPath->empty()) {
      Candidate = {.path = *Params.rootPath, .source = "rootPath"};
    } else {
      if (Params.workspaceFoldersError)
        return fallback(CLI, LaunchCWD,
                        invalidCandidate("workspaceFolders",
                                         *Params.workspaceFoldersError));
      if (Params.workspaceFolders && Params.workspaceFolders->size() == 1) {
        Candidate = {
            .path = Params.workspaceFolders->front().uri.file().str(),
            .source = "workspaceFolders[0]",
        };
      } else {
        Candidate = {.path = LaunchCWD, .source = "launch directory"};
      }
    }
  }

  if (std::optional<std::string> Reason = unusableReason(Candidate.path))
    return fallback(CLI, LaunchCWD, unusableCandidate(Candidate, *Reason));

  const std::filesystem::path ProjectPath = Candidate.path / ".nixd.json";
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> Input =
      llvm::MemoryBuffer::getFile(ProjectPath.string());
  if (!Input) {
    const std::error_code EC = Input.getError();
    if (EC == std::errc::no_such_file_or_directory)
      return fallback(CLI, LaunchCWD);
    return fallback(CLI, LaunchCWD, loadFailure(ProjectPath, EC.message()));
  }

  try {
    ConfigurationPatch Patch =
        nixd::fromJSON<ConfigurationPatch>(nixd::parse((*Input)->getBuffer()));
    return {
        .baseConfiguration = overlay(CLI.baseConfiguration, Patch),
        .executionCWD = Candidate.path,
        .warning = std::nullopt,
    };
  } catch (LLVMErrorException &Err) {
    const std::string Reason =
        llvm::formatv("{0}: {1}", Err.what(), llvm::toString(Err.takeError()))
            .str();
    return fallback(CLI, LaunchCWD, loadFailure(ProjectPath, Reason));
  }
}
