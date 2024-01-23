#pragma once

#include <cstdint>
#include <string_view>

namespace nixf {

/// \brief A point in the source file.
///
/// This class is used to represent a point in the source file. And it shall be
/// constructed by Lexer, to keep Line & Column information correct.
/// \see Lexer::consume(std::size_t)
class Point {
  int64_t Line;
  int64_t Column;
  std::size_t Offset;
  friend class Lexer;
  Point(int64_t Line, int64_t Column, std::size_t Offset)
      : Line(Line), Column(Column), Offset(Offset) {}

public:
  friend bool operator==(const Point &LHS, const Point &RHS) {
    return LHS.Line == RHS.Line && LHS.Column == RHS.Column &&
           LHS.Offset == RHS.Offset;
  }
  Point() = default;

  Point(const Point &) = default;
  Point &operator=(const Point &) = default;
  Point(Point &&) = default;

  /// \brief Check if the point is at the given position.
  [[nodiscard]] bool isAt(int64_t Line, int64_t Column,
                          std::size_t Offset) const {
    return this->Line == Line && this->Column == Column &&
           this->Offset == Offset;
  }

  /// \brief Line number, starting from 0.
  ///
  /// Currently we only accept LF as the line terminator.
  [[nodiscard]] int64_t line() const { return Line; }

  /// \brief Column number, starting from 0.
  [[nodiscard]] int64_t column() const { return Column; }

  /// \brief Offset in the source file, starting from 0.
  [[nodiscard]] std::size_t offset() const { return Offset; }
};

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
