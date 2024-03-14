#pragma once

#include <cassert>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace bc {

template <class T> void readBytecode(std::string_view &Data, T &Obj) {
  static_assert(!std::is_same_v<T, T>,
                "No readBytecode implementation for this type");
}

/// \brief Basic primitives. Deocde from bytes by `memcpy`.
template <class T>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
void readBytecode(std::string_view &Data, T &Obj) {
  assert(Data.size() >= sizeof(T));
  std::memcpy(&Obj, Data.begin(), sizeof(T));
  Data = Data.substr(sizeof(T));
}

template <class T>
void readBytecode(std::string_view &Data, std::vector<T> &Obj) {
  size_t Size;
  readBytecode(Data, Size);
  Obj.resize(Size);
  for (auto &E : Obj)
    readBytecode(Data, E);
}

template <>
void readBytecode<std::string>(std::string_view &Data, std::string &Obj);

template <class T> T eat(std::string_view &Data) {
  T Obj;
  readBytecode(Data, Obj);
  return Obj;
}

} // namespace bc
