#include "nixbc/Origin.h"

#include <cassert>

namespace nixbc {

void readBytecode(std::string_view &Data, Origin &Obj) {}

void writeBytecode(std::ostream &OS, const Origin &O) {}

void readBytecode(std::string_view &Data, OriginPath &Obj) {}

void writeBytecode(std::ostream &OS, const OriginPath &O) {}

} // namespace nixbc
