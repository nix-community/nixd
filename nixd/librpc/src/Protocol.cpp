#include "nixd/rpc/Protocol.h"

#include <bc/Read.h>
#include <bc/Write.h>

#include <cstring>
#include <sstream>

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

} // namespace nixd::rpc
