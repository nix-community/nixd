#pragma once

#include "Nodes.h"
#include "Origin.h"

#include <ostream>
#include <string_view>
#include <type_traits>

namespace nixbc {

/// \brief Basic primitives. Trivial data types are just written to a stream.
template <class T>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
void serialize(std::ostream &OS, const T &Data) {
  OS.write(reinterpret_cast<const char *>(&Data), sizeof(T));
}

inline void serializeNodeMeta(std::ostream &OS, ExprKind Kind,
                              std::uintptr_t Ptr) {
  serialize(OS, Ptr);
  serialize(OS, Kind);
}

void serialize(std::ostream &OS, std::string_view S);

void serialize(std::ostream &OS, const OriginPath &O);

void serialize(std::ostream &OS, const Origin &O);

} // namespace nixbc
