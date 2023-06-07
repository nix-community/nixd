#include "lspserver/Protocol.h"

#include <llvm/Support/JSON.h>

#include <list>
#include <optional>
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

  struct Eval {
    /// Nix installables that will be used for root translation unit.
    std::optional<InstallableConfigurationItem> target;

    /// The depth you'd like to eval *after* reached "installable" target.
    std::optional<int> depth;
    /// Number of workers forking
    /// defaults to std::thread::hardware_concurrency
    std::optional<int> workers;
  };

  std::optional<Eval> eval;

  struct Formatting {
    std::optional<std::string> command;
  };
  std::optional<Formatting> formartting;

  struct Options {
    std::optional<bool> enable;
    std::optional<InstallableConfigurationItem> target;
  };

  std::optional<Options> options;

  [[nodiscard]] std::string getFormatCommand() const {
    if (formartting.has_value() && formartting->command.has_value())
      return formartting->command.value();
    return "nixpkgs-fmt";
  }

  [[nodiscard]] int getEvalDepth() const {
    if (eval.has_value() && eval->depth.has_value())
      return eval->depth.value();
    return 0;
  }

  [[nodiscard]] int getNumWorkers() const {
    if (eval.has_value() && eval->workers.has_value())
      return eval->workers.value();
    return static_cast<int>(std::thread::hardware_concurrency());
  }

  /// Get installable arguments specified in this config, fallback to file \p
  /// Fallback if 'installable' is not set.
  [[nodiscard]] static std::tuple<std::list<std::string>, std::string>
  getInstallable(const InstallableConfigurationItem &);
};
bool fromJSON(const llvm::json::Value &Params, TopLevel::Eval &R,
              llvm::json::Path P);
bool fromJSON(const llvm::json::Value &Params, TopLevel::Formatting &R,
              llvm::json::Path P);
bool fromJSON(const llvm::json::Value &Params, TopLevel::Options &R,
              llvm::json::Path P);
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

struct AttrPathParams {
  std::string Path;
};

bool fromJSON(const llvm::json::Value &, AttrPathParams &, llvm::json::Path);

llvm::json::Value toJSON(const AttrPathParams &);

} // namespace ipc
} // namespace nixd
