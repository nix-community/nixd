#pragma once

#include "nixf/Basic/Range.h"

#include <cstdint>
#include <memory>
#include <string_view>

namespace nixf {

/// \brief Represents a comment in source code
class Comment {
public:
  enum CommentKind : std::uint8_t {
    CK_LineComment,  // # comment
    CK_BlockComment, // /* comment */
  };

private:
  CommentKind Kind;
  LexerCursorRange Range;
  std::string_view Text;

public:
  Comment(CommentKind Kind, LexerCursorRange Range, std::string_view Text)
      : Kind(Kind), Range(Range), Text(Text) {}

  [[nodiscard]] CommentKind kind() const { return Kind; }
  [[nodiscard]] LexerCursorRange range() const { return Range; }
  [[nodiscard]] std::string_view text() const { return Text; }

  /// \brief Check if this is a directive comment (e.g., # nixf-ignore: ...)
  [[nodiscard]] bool isDirective() const {
    return Text.find("nixf-ignore:") != std::string_view::npos ||
           Text.find("nixf-disable:") != std::string_view::npos;
  }
};

using CommentPtr = std::shared_ptr<Comment>;

} // namespace nixf
