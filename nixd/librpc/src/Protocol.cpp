#include "nixd/rpc/Protocol.h"

#include <bc/Read.h>
#include <bc/Write.h>

namespace nixd::rpc {

using bc::readBytecode;
using bc::writeBytecode;

void writeBytecode(std::ostream &OS, const RegisterBCParams &Params) {
  writeBytecode(OS, Params.Shm);
  writeBytecode(OS, Params.BasePath);
  writeBytecode(OS, Params.CachePath);
  writeBytecode(OS, Params.Size);
}

void readBytecode(std::string_view &Data, RegisterBCParams &Params) {
  readBytecode(Data, Params.Shm);
  readBytecode(Data, Params.BasePath);
  readBytecode(Data, Params.CachePath);
  readBytecode(Data, Params.Size);
}

void writeBytecode(std::ostream &OS, const ExprValueParams &Params) {
  writeBytecode(OS, Params.ExprID);
}

void readBytecode(std::string_view &Data, ExprValueParams &Params) {
  readBytecode(Data, Params.ExprID);
}

void writeBytecode(std::ostream &OS, const ExprValueResponse &Params) {
  writeBytecode(OS, Params.ValueKind);
  writeBytecode(OS, Params.ValueID);
  writeBytecode(OS, Params.ResultKind);
}
void readBytecode(std::string_view &Data, ExprValueResponse &Params) {
  readBytecode(Data, Params.ValueKind);
  readBytecode(Data, Params.ValueID);
  readBytecode(Data, Params.ResultKind);
}

} // namespace nixd::rpc
