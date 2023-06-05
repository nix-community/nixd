#include "lspserver/Protocol.h"

#include <llvm/Support/JSON.h>

#include <list>
#include <thread>

// Extension to `lspserver`
namespace lspserver {

llvm::json::Value toJSON(const CompletionContext &R);

llvm::json::Value toJSON(const TextDocumentPositionParams &R);

llvm::json::Value toJSON(const CompletionParams &R);

bool fromJSON(const llvm::json::Value &Params, CompletionItem &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, CompletionList &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, Hover &R, llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, MarkupContent &R,
              llvm::json::Path P);

bool fromJSON(const llvm::json::Value &Params, Location &R, llvm::json::Path P);

} // namespace lspserver

namespace nixd {
namespace configuration {

struct InstallableConfigurationItem {
  std::vector<std::string> args;
  std::string installable;
};

struct TopLevel {
  /// Nix installables that will be used for root translation unit.
  std::optional<InstallableConfigurationItem> installable;

  /// The depth you'd like to eval *after* reached "installable" target.
  std::optional<int> evalDepth;

  /// Number of workers forking
  /// defaults to std::thread::hardware_concurrency
  std::optional<int> numWorkers;

  int getNumWorkers() {
    return numWorkers.value_or(std::thread::hardware_concurrency());
  }

  /// Get installable arguments specified in this config, fallback to file \p
  /// Fallback if 'installable' is not set.
  [[nodiscard]] std::tuple<std::list<std::string>, std::string>
  getInstallable(std::string Fallback) const;
};

bool fromJSON(const llvm::json::Value &Params, TopLevel &R, llvm::json::Path P);
bool fromJSON(const llvm::json::Value &Params, InstallableConfigurationItem &R,
              llvm::json::Path P);
} // namespace configuration

namespace ipc {

using WorkspaceVersionTy = uint64_t;

/// Messages sent by workers must tell it's version
struct WorkerMessage {
  WorkspaceVersionTy WorkspaceVersion;
};

bool fromJSON(const llvm::json::Value &, WorkerMessage &, llvm::json::Path);
llvm::json::Value toJSON(const WorkerMessage &);

/// Sent by the worker process, tell us it has prepared diagnostics
/// <----
struct Diagnostics : WorkerMessage {
  std::vector<lspserver::PublishDiagnosticsParams> Params;
};

bool fromJSON(const llvm::json::Value &, lspserver::PublishDiagnosticsParams &,
              llvm::json::Path);

bool fromJSON(const llvm::json::Value &, Diagnostics &, llvm::json::Path);
llvm::json::Value toJSON(const Diagnostics &);

struct Completion : WorkerMessage {
  lspserver::CompletionList List;
};

} // namespace ipc
} // namespace nixd
