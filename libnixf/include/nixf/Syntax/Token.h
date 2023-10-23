#pragma once

#include "RawSyntax.h"
#include "Trivia.h"
#include <string>

namespace nixf {

namespace tok {
enum TokenKind {
  tok_eof,

  tok_int,
  tok_float,

  tok_dquote, // "
  tok_string_part,
  tok_dollar_curly,  // ${
  tok_string_escape, // escaped string, e.g. \r \n \x \"

  tok_err,
};

} // namespace tok

class Token : public RawNode {
  const tok::TokenKind Kind;
  const std::string Content;
  const std::unique_ptr<Trivia> LeadingTrivia;
  const std::unique_ptr<Trivia> TrailingTrivia;

public:
  Token(tok::TokenKind Kind, const std::string &Content,
        std::unique_ptr<Trivia> LeadingTrivia,
        std::unique_ptr<Trivia> TrailingTrivia)
      : Kind(Kind), Content(Content), LeadingTrivia(std::move(LeadingTrivia)),
        TrailingTrivia(std::move(TrailingTrivia)) {
    Length = Content.length();
    if (LeadingTrivia)
      Length += LeadingTrivia->getLength();
    if (TrailingTrivia)
      Length += TrailingTrivia->getLength();
  }

  tok::TokenKind getKind() { return Kind; }

  Trivia *getLeadingTrivia() { return LeadingTrivia.get(); }
  Trivia *getTrailingTrivia() { return TrailingTrivia.get(); }

  std::string_view getContent() { return Content; }

  void dump(std::ostream &OS) const override {
    if (LeadingTrivia)
      LeadingTrivia->dump(OS);
    OS << Content;
    if (TrailingTrivia)
      TrailingTrivia->dump(OS);
  }
};

} // namespace nixf
