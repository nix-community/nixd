#pragma once

#include "IPC.h"

#include <llvm/Support/JSON.h>

#include <cstdint>

namespace nixd::ipc {

bool fromJSON(const llvm::json::Value &, WorkerMessage &, llvm::json::Path);
llvm::json::Value toJSON(const WorkerMessage &);

bool fromJSON(const llvm::json::Value &, Diagnostics &, llvm::json::Path);
llvm::json::Value toJSON(const Diagnostics &);

} // namespace nixd::ipc
