#include "nixbc/Deserialize.h"
#include "nixbc/Origin.h"

namespace nixbc {

void deserialize(std::string_view &Data, std::string_view Obj) {
  auto Size = eat<std::string_view::size_type>(Data);
  Obj = Data.substr(0, Size);
  Data = Data.substr(Size);
}

} // namespace nixbc
