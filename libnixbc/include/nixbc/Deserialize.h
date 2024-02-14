#pragma once

#include <cassert>
#include <cstring>
#include <span>
#include <type_traits>

namespace nixbc {

/// \brief Basic primitives. Deocde from bytes by `memcpy`.
template <class T>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
T deserialize(std::span<const char> Data) {
  T Obj;
  assert(Data.size() >= sizeof(T));
  std::memcpy(&Obj, Data.begin(), sizeof(T));
  return Obj;
}

} // namespace nixbc
