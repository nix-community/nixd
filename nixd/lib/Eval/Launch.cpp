#include "nixd/Eval/Launch.h"
#include "nixd/CommandLine/Options.h"

#include <llvm/Support/CommandLine.h>

#include <algorithm>

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

std::unique_ptr<AttrSetClientProc>
nixd::startAttrSetEval(const std::string &Name) {
  return std::make_unique<AttrSetClientProc>([&Name]() {
    freopen(Name.c_str(), "w", stderr);
    return execl(AttrSetClient::getExe(), "nixd-attrset-eval", nullptr);
  });
}

std::unique_ptr<AttrSetClientProc> nixd::startNixpkgs() {
  return startAttrSetEval(NixpkgsWorkerStderr);
}

std::unique_ptr<AttrSetClientProc> nixd::startOption(const std::string &Name) {
  std::string NewName = NULL_DEVICE;
  if (OptionWorkerStderr.getNumOccurrences()) {
    // `#:schema` workers are named by module path; keep it one file name.
    std::string File = Name;
    std::replace(File.begin(), File.end(), '/', '%');
    NewName = OptionWorkerStderr.getValue() + "/" + File;
  }
  return startAttrSetEval(NewName);
}
