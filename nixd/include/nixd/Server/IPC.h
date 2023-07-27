#pragma once

#include "lspserver/Protocol.h"

namespace nixd::ipc {

using WorkspaceVersionTy = uint64_t;

/// Messages sent by workers must tell it's version
struct WorkerMessage {
  WorkspaceVersionTy WorkspaceVersion;
};

/// Sent by the worker process, tell us it has prepared diagnostics
/// <----
struct Diagnostics : WorkerMessage {
  std::vector<lspserver::PublishDiagnosticsParams> Params;
};

struct AttrPathParams {
  std::string Path;
};

bool fromJSON(const llvm::json::Value &, AttrPathParams &, llvm::json::Path);

llvm::json::Value toJSON(const AttrPathParams &);

} // namespace nixd::ipc
