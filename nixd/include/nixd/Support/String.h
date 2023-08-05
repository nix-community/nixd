#pragma once

#include <string>

namespace nixd {

inline void truncateString(std::string &Str, size_t Size) {
  if (Str.size() > Size) {
    Str.resize(Size);
  }
}

} // namespace nixd
