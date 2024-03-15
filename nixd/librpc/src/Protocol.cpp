#include "nixd/rpc/Protocol.h"

#include <bc/Read.h>
#include <bc/Write.h>

namespace nixd::rpc {

using bc::readBytecode;
using bc::writeBytecode;
using namespace llvm::json;

Value toJSON(const RegisterBCParams &Params) {
  return Object{{"Shm", Params.Shm},
                {"BasePath", Params.BasePath},
                {"CachePath", Params.CachePath},
                {"Size", Params.Size}};
}

bool fromJSON(const Value &Params, RegisterBCParams &R, Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("Shm", R.Shm) && O.map("BasePath", R.BasePath) &&
         O.map("CachePath", R.CachePath) && O.map("Size", R.Size);
}

Value toJSON(const ExprValueParams &Params) {
  return Object{{"ExprID", Params.ExprID}};
}

bool fromJSON(const Value &Params, ExprValueParams &R, Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("ExprID", R.ExprID);
}

Value toJSON(const ExprValueResponse &Params) {
  return Object{{"ResultKind", Params.ResultKind},
                {"ValueID", Params.ValueID},
                {"ValueKind", Params.ValueKind}};
}

bool fromJSON(const Value &Params, ExprValueResponse &R, Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("ResultKind", R.ResultKind) &&
         O.map("ValueID", R.ValueID) && O.map("ValueKind", R.ValueKind);
}

} // namespace nixd::rpc
