#include "lspserver/Protocol.h"
#include "nixd/Server.h"

#include <llvm/Support/JSON.h>

namespace nixd {

using namespace llvm::json;

namespace configuration {

bool fromJSON(const Value &Params, TopLevel &R, Path P) {
  const auto *PA = Params.getAsArray();
  auto X = PA->front();
  ObjectMapper O(X, P);
  return O && O.map("installable", R.installable);
}

bool fromJSON(const Value &Params, nix::Strings &R, Path P) {
  std::vector<std::string> RVec{std::begin(R), std::end(R)};
  return fromJSON(Params, RVec, P);
}

bool fromJSON(const Value &Params, InstallableConfigurationItem &R, Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("args", R.args) && O.map("installable", R.installable);
}

} // namespace configuration

namespace ipc {
bool fromJSON(const Value &Params, WorkerMessage &R, Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("WorkspaceVersion", R.WorkspaceVersion);
}

Value toJSON(const WorkerMessage &R) {
  return Object{{"WorkspaceVersion", R.WorkspaceVersion}};
}

bool fromJSON(const Value &Params, Diagnostics &R, Path P) {
  WorkerMessage &Base = R;
  ObjectMapper O(Params, P);
  return fromJSON(Params, Base, P) && O.map("Params", R.Params);
}

Value toJSON(const Diagnostics &R) {
  Value Base = toJSON(WorkerMessage(R));
  Base.getAsObject()->insert({"Params", R.Params});
  return Base;
}

} // namespace ipc

} // namespace nixd

namespace lspserver {

using llvm::json::ObjectMapper;
using llvm::json::Value;

bool fromJSON(const Value &Params, Diagnostic &R, llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("range", R.range) && O.map("severity", R.severity) &&
         O.map("code", R.code) && O.map("source", R.source) &&
         O.map("message", R.message) && O.mapOptional("category", R.category);
}

bool fromJSON(const Value &Params, PublishDiagnosticsParams &R,
              llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("uri", R.uri) && O.mapOptional("version", R.version) &&
         O.map("diagnostics", R.diagnostics);
}

} // namespace lspserver
