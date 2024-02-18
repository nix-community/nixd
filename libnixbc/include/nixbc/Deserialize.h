#pragma once

#include "Origin.h"

#include <cassert>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace nixbc {

/// \brief Basic primitives. Deocde from bytes by `memcpy`.
template <class T>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
void deserialize(std::string_view &Data, T &Obj) {
  assert(Data.size() >= sizeof(T));
  std::memcpy(&Obj, Data.begin(), sizeof(T));
  Data = Data.substr(sizeof(T));
}

void deserialize(std::string_view &Data, std::string_view Obj);

template <class T> T eat(std::string_view &Data) {
  T Obj;
  deserialize(Data, Obj);
  return Obj;
}

inline void deserialize(std::string_view &Data, OriginPath &Obj) {
  deserialize(Data, Obj.path());
}

} // namespace nixbc
