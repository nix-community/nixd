#include "nixd/Server/IPCSerialization.h"
#include "nixd/Server/ConfigSerialization.h"
#include "nixd/Server/IPC.h"
#include "nixd/Support/JSONSerialization.h"

namespace nixd::ipc {

using namespace llvm::json;

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

bool fromJSON(const Value &Params, AttrPathParams &R, Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("Path", R.Path);
}

Value toJSON(const AttrPathParams &R) { return Object{{"Path", R.Path}}; }

Value toJSON(const EvalParams &R) {
  return Object{{"Eval", R.Eval}, {"WorkspaceVersion", R.WorkspaceVersion}};
}

bool fromJSON(const Value &Params, EvalParams &R, Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("Eval", R.Eval) &&
         O.map("WorkspaceVersion", R.WorkspaceVersion);
}

} // namespace nixd::ipc
