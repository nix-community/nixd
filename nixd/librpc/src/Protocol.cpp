#include "nixd/rpc/Protocol.h"

#include <bc/Read.h>
#include <bc/Write.h>
#include <llvm/Support/JSON.h>

using namespace nixd;
using namespace rpc;
using namespace llvm::json;

Value rpc::toJSON(const RegisterBCParams &Params) {
  return Object{{"Shm", Params.Shm},
                {"BasePath", Params.BasePath},
                {"CachePath", Params.CachePath},
                {"Size", Params.Size}};
}

bool rpc::fromJSON(const Value &Params, RegisterBCParams &R, Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("Shm", R.Shm) && O.map("BasePath", R.BasePath) &&
         O.map("CachePath", R.CachePath) && O.map("Size", R.Size);
}

Value rpc::toJSON(const ExprValueParams &Params) {
  return Object{{"ExprID", Params.ExprID}};
}

bool rpc::fromJSON(const Value &Params, ExprValueParams &R, Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("ExprID", R.ExprID);
}

llvm::json::Value rpc::toJSON(const DerivationDescription &Params) {
  return Object{
      {"PName", Params.PName},
      {"Version", Params.Version},
      {"Position", Params.Position},
      {"Description", Params.Description},
  };
}

bool rpc::fromJSON(const llvm::json::Value &Params, DerivationDescription &R,
                   llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O && O.map("PName", R.PName) && O.map("Version", R.Version) &&
         O.map("Position", R.Position) && O.map("Description", R.Description);
}

Value rpc::toJSON(const ExprValueResponse &Params) {
  return Object{
      {"ResultKind", Params.ResultKind},
      {"ErrorMsg", Params.ErrorMsg},
      {"Description", Params.Description},
      {"DrvDesc", Params.DrvDesc},
  };
}

bool rpc::fromJSON(const Value &Params, ExprValueResponse &R, Path P) {
  ObjectMapper O(Params, P);
  // clang-format off
  return O && O.map("ResultKind", R.ResultKind)
           && O.map("ErrorMsg", R.ErrorMsg)
           && O.map("Description", R.Description)
           && O.map("DrvDesc", R.DrvDesc);
  // clang-format on
}
