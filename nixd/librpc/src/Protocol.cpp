#include "nixd/rpc/Protocol.h"

#include <bc/Read.h>
#include <bc/Write.h>

namespace nixd::rpc {

using bc::readBytecode;
using bc::writeBytecode;

void writeBytecode(std::ostream &OS, const RegisterBCParams &Params) {
  writeBytecode(OS, Params.Shm);
  writeBytecode(OS, Params.Size);
  writeBytecode(OS, Params.BCID);
}

void readBytecode(std::string_view &Data, RegisterBCParams &Params) {
  readBytecode(Data, Params.Shm);
  readBytecode(Data, Params.Size);
  readBytecode(Data, Params.BCID);
}

void writeBytecode(std::ostream &OS, const ExprValueParams &Params) {
  writeBytecode(OS, Params.ExprID);
  writeBytecode(OS, Params.BCID);
}

void readBytecode(std::string_view &Data, ExprValueParams &Params) {
  readBytecode(Data, Params.ExprID);
  readBytecode(Data, Params.BCID);
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
