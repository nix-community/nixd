#include "nixd/Eval/Launch.h"
#include "nixd/CommandLine/Options.h"
#include "nixd/Eval/Spawn.h"

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
                            std::unique_ptr<AttrSetClientProc> &Worker) {
  spawnAttrSetEval(Name, Worker);
}

void nixd::startNixpkgs(std::unique_ptr<AttrSetClientProc> &NixpkgsEval) {
  startAttrSetEval(NixpkgsWorkerStderr, NixpkgsEval);
}

void nixd::startOption(const std::string &Name,
                       std::unique_ptr<AttrSetClientProc> &Worker) {
  std::string NewName = NULL_DEVICE;
  if (OptionWorkerStderr.getNumOccurrences())
    NewName = OptionWorkerStderr.getValue() + "/" + Name;
  startAttrSetEval(NewName, Worker);
}
