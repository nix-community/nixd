#pragma once

#include "lspserver/Protocol.h"
#include "nixd/Server/Config.h"

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

struct EvalParams {
  WorkspaceVersionTy WorkspaceVersion;
  configuration::TopLevel::Eval Eval;
};

} // namespace nixd::ipc
