#pragma once

#include <cstdint>

namespace nixf {

struct OffsetRange {
  const char *Begin;
  const char *End;

  OffsetRange() = default;

  OffsetRange(const char *Begin, const char *End) : Begin(Begin), End(End) {}
  explicit OffsetRange(const char *Pos) : Begin(Pos), End(Pos) {}
};

} // namespace nixf
