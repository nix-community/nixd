#pragma once

#include <ostream>
#include <sstream>
#include <type_traits>

namespace bc {

/// \brief Basic primitives. Trivial data types are just written to a stream.
template <class T>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
void writeBytecode(std::ostream &OS, const T &Data) {
  OS.write(reinterpret_cast<const char *>(&Data), sizeof(T));
}

void writeBytecode(std::ostream &OS, const std::string_view &Data);

inline void writeBytecode(std::ostream &OS, const std::string &Data) {
  writeBytecode(OS, std::string_view(Data));
}

template <class T> std::string toBytecode(const T &Data) {
  std::ostringstream OS;
  writeBytecode(OS, Data);
  return OS.str();
}

} // namespace bc
