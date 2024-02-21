#pragma once

#include <ostream>
#include <type_traits>

namespace bc {

template <class T> void writeBytecode(std::ostream &OS, const T &Data) {
  static_assert(!std::is_same_v<T, T> &&
                "No writeBytecode implementation for this type");
}

/// \brief Basic primitives. Trivial data types are just written to a stream.
template <class T>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
void writeBytecode(std::ostream &OS, const T &Data) {
  OS.write(reinterpret_cast<const char *>(&Data), sizeof(T));
}

template <>
void writeBytecode<std::string_view>(std::ostream &OS,
                                     const std::string_view &Data);

template <>
inline void writeBytecode<std::string>(std::ostream &OS,
                                       const std::string &Data) {
  writeBytecode(OS, std::string_view(Data));
}

} // namespace bc
