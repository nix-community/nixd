#pragma once

#include <cstdint>
#include <string_view>

namespace nixf {

struct Point {
  int64_t Line;
  int64_t Column;
  std::size_t Offset;
};

inline bool operator==(const Point &LHS, const Point &RHS) {
  return LHS.Line == RHS.Line && LHS.Column == RHS.Column &&
         LHS.Offset == RHS.Offset;
}

class RangeTy {
  Point Begin;
  Point End;

public:
  RangeTy() = default;

  RangeTy(Point Begin, Point End) : Begin(Begin), End(End) {}
  explicit RangeTy(Point Pos) : Begin(Pos), End(Pos) {}

  [[nodiscard]] Point begin() const { return Begin; }
  [[nodiscard]] Point end() const { return End; }
};

} // namespace nixf
