#pragma once

#include "nixd/Eval/AttrSetClient.h"

#include <memory>
#include <string_view>

namespace nixd {

/// Launches an attrset evaluator process whose stderr is redirected to
/// `WorkerStderr`.
void spawnAttrSetEval(std::string_view WorkerStderr,
                      std::unique_ptr<AttrSetClientProc> &Worker);

} // namespace nixd
