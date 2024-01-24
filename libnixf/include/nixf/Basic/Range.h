#pragma once

#include <cstdint>
#include <string_view>

namespace nixf {

class Position {
  int64_t Line;
  int64_t Column;

public:
  Position() = default;
  Position(int64_t Line, int64_t Column) : Line(Line), Column(Column) {}

  [[nodiscard]] int64_t line() const { return Line; }
  [[nodiscard]] int64_t column() const { return Column; }

  friend bool operator==(const Position &LHS, const Position &RHS) {
    return LHS.Line == RHS.Line && LHS.Column == RHS.Column;
  }

  friend bool operator<(const Position &LHS, const Position &RHS) {
    return LHS.Line < RHS.Line ||
           (LHS.Line == RHS.Line && LHS.Column < RHS.Column);
  }

  friend bool operator<=(const Position &LHS, const Position &RHS) {
    return LHS < RHS || LHS == RHS;
  }
};

class PositionRange {
  Position Begin;
  Position End;

public:
  PositionRange() = default;

  PositionRange(Position Begin, Position End) : Begin(Begin), End(End) {}
  explicit PositionRange(Position Pos) : Begin(Pos), End(Pos) {}

  [[nodiscard]] Position begin() const { return Begin; }
  [[nodiscard]] Position end() const { return End; }

  /// \brief Check if the range contains another range.
  [[nodiscard]] bool contains(const PositionRange &Pos) const {
    return Begin <= Pos.Begin && Pos.End <= End;
  }
};

/// \brief A point in the source file.
///
/// This class is used to represent a point in the source file. And it shall be
/// constructed by Lexer, to keep Line & Column information correct.
/// \see Lexer::consume(std::size_t)
class LexerCursor {
  int64_t Line;
  int64_t Column;
  std::size_t Offset;
  friend class Lexer;
  LexerCursor(int64_t Line, int64_t Column, std::size_t Offset)
      : Line(Line), Column(Column), Offset(Offset) {}

public:
  friend bool operator==(const LexerCursor &LHS, const LexerCursor &RHS) {
    return LHS.Line == RHS.Line && LHS.Column == RHS.Column &&
           LHS.Offset == RHS.Offset;
  }
  LexerCursor() = default;

  LexerCursor(const LexerCursor &) = default;
  LexerCursor &operator=(const LexerCursor &) = default;
  LexerCursor(LexerCursor &&) = default;

  /// \brief Check if the point is at the given position.
  [[nodiscard]] bool isAt(int64_t Line, int64_t Column,
                          std::size_t Offset) const {
    return this->line() == Line && this->column() == Column &&
           this->Offset == Offset;
  }

  /// \brief Line number, starting from 0.
  ///
  /// Currently we only accept LF as the line terminator.
  [[nodiscard]] int64_t line() const { return Line; }

  /// \brief Column number, starting from 0.
  [[nodiscard]] int64_t column() const { return Column; }

  /// \brief Position in the source file. (`Line` + `Column`)
  [[nodiscard]] Position position() const { return {Line, Column}; }

  /// \brief Offset in the source file, starting from 0.
  [[nodiscard]] std::size_t offset() const { return Offset; }
};

class LexerCursorRange {
  LexerCursor Begin;
  LexerCursor End;

public:
  LexerCursorRange() = default;

  LexerCursorRange(LexerCursor Begin, LexerCursor End)
      : Begin(Begin), End(End) {}
  explicit LexerCursorRange(LexerCursor Pos) : Begin(Pos), End(Pos) {}

  [[nodiscard]] LexerCursor begin() const { return Begin; }
  [[nodiscard]] LexerCursor end() const { return End; }

  [[nodiscard]] bool contains(const LexerCursorRange &Pos) const {
    return positionRange().contains(Pos.positionRange());
  }

  [[nodiscard]] PositionRange positionRange() const {
    return {Begin.position(), End.position()};
  }
};

} // namespace nixf
