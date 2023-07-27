#pragma once

#include "IPC.h"

#include <llvm/Support/JSON.h>

#include <cstdint>

namespace nixd::ipc {

llvm::json::Value toJSON(const AttrPathParams &);

llvm::json::Value toJSON(const EvalParams &);

bool fromJSON(const llvm::json::Value &, WorkerMessage &, llvm::json::Path);
llvm::json::Value toJSON(const WorkerMessage &);

bool fromJSON(const llvm::json::Value &, Diagnostics &, llvm::json::Path);
llvm::json::Value toJSON(const Diagnostics &);

bool fromJSON(const llvm::json::Value &, AttrPathParams &, llvm::json::Path);

bool fromJSON(const llvm::json::Value &, EvalParams &, llvm::json::Path);

} // namespace nixd::ipc
