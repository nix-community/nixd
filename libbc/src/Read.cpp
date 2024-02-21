#include "bc/Read.h"

namespace bc {

template <>
void readBytecode<std::string>(std::string_view &Data, std::string &Obj) {
  size_t Size;
  readBytecode(Data, Size);
  Obj.resize(Size);
  std::memcpy(Obj.data(), Data.data(), Size);
  Data = Data.substr(Size);
}

} // namespace bc
