#pragma once

#include "Origin.h"

#include <cstdint>

namespace nixbc {

struct FileHeader {
  static constexpr int MagicValue = 0x72A17086;
  uint32_t Magic;
  uint32_t Version;
  /* Origin */
};

} // namespace nixbc
