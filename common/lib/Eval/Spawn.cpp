#include "nixd/Eval/Spawn.h"

#include <unistd.h>

#include <cstdio>
#include <string>

namespace nixd {

namespace {
constexpr std::string_view NullDevice{"/dev/null"};
}

void spawnAttrSetEval(std::string_view WorkerStderr,
                      std::unique_ptr<AttrSetClientProc> &Worker) {
  std::string Path = WorkerStderr.empty() ? std::string(NullDevice)
                                          : std::string(WorkerStderr);
  Worker = std::make_unique<AttrSetClientProc>([Path = std::move(Path)]() {
    freopen(Path.c_str(), "w", stderr);
    return execl(AttrSetClient::getExe(), "nixd-attrset-eval", nullptr);
  });
}

} // namespace nixd
