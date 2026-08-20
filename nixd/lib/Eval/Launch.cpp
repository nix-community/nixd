#include "nixd/Eval/Launch.h"
#include "nixd/CommandLine/Options.h"

#include <llvm/Support/CommandLine.h>

using namespace llvm::cl;
using namespace nixd;

namespace {

#define NULL_DEVICE "/dev/null"

opt<std::string> OptionWorkerStderr{
    "option-worker-stderr", desc("Directory to write options worker stderr"),
    cat(NixdCategory), init(NULL_DEVICE)};

opt<std::string> NixpkgsWorkerStderr{
    "nixpkgs-worker-stderr",
    desc("Writable file path for nixpkgs worker stderr (debugging)"),
    cat(NixdCategory), init(NULL_DEVICE)};

} // namespace

void nixd::startAttrSetEval(const std::string &Name,
                            std::unique_ptr<AttrSetClientProc> &Worker,
                            const std::filesystem::path &CWD,
                            std::function<void()> OnDeath) {
  const std::filesystem::path ConfiguredExecutable = AttrSetClient::getExe();
  const std::filesystem::path WorkingDirectory = std::filesystem::absolute(CWD);
  const std::filesystem::path Executable =
      ConfiguredExecutable.is_absolute()
          ? ConfiguredExecutable
          : WorkingDirectory / ConfiguredExecutable;
  Worker = std::make_unique<AttrSetClientProc>(
      ExecSpec{
          .Executable = Executable,
          .Arguments = {"nixd-attrset-eval"},
          .Stderr = Name,
          .WorkingDirectory = WorkingDirectory,
      },
      std::move(OnDeath));
}

void nixd::startNixpkgs(std::unique_ptr<AttrSetClientProc> &NixpkgsEval,
                        const std::filesystem::path &CWD,
                        std::function<void()> OnDeath) {
  startAttrSetEval(NixpkgsWorkerStderr, NixpkgsEval, CWD, std::move(OnDeath));
}

void nixd::startOption(const std::string &Name,
                       std::unique_ptr<AttrSetClientProc> &Worker,
                       const std::filesystem::path &CWD,
                       std::function<void()> OnDeath) {
  std::string NewName = NULL_DEVICE;
  if (OptionWorkerStderr.getNumOccurrences())
    NewName = OptionWorkerStderr.getValue() + "/" + Name;
  startAttrSetEval(NewName, Worker, CWD, std::move(OnDeath));
}
