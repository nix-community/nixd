#pragma once

#include <cstdint>
#include <string_view>

namespace nixf {

struct OffsetRange {
  const char *Begin;
  const char *End;

  OffsetRange() = default;

  OffsetRange(const char *Begin, const char *End) : Begin(Begin), End(End) {}
  explicit OffsetRange(const char *Pos) : Begin(Pos), End(Pos) {}
  operator std::string_view() const { return {Begin, End}; }
  [[nodiscard]] std::string_view view() const { return {Begin, End}; }
};

} // namespace nixf
