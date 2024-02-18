#include "nixbc/Serialize.h"

#include <cassert>

namespace nixbc {

void serialize(std::ostream &OS, std::string_view S) {
  serialize(OS, S.size());
  OS.write(S.data(), static_cast<std::streamsize>(S.size()));
}

void serialize(std::ostream &OS, const OriginPath &O) {
  serialize(OS, std::string_view(O.path()));
}

void serialize(std::ostream &OS, const Origin &O) {
  serialize(OS, O.kind());
  switch (O.kind()) {
  case Origin::OK_Path:
    serialize(OS, static_cast<const OriginPath &>(O));
    break;
  case Origin::OK_None:
  default:
    break;
  }
  assert(false && "Unhandled origin kind");
  __builtin_unreachable();
}

} // namespace nixbc
