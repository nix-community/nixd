#include "bc/Write.h"

namespace bc {

void writeBytecode(std::ostream &OS, const std::string_view &Data) {
  writeBytecode(OS, static_cast<std::size_t>(Data.size()));
  OS.write(Data.data(), static_cast<std::streamsize>(Data.size()));
}

} // namespace bc
