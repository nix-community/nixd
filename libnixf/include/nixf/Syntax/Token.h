#pragma once

#include "RawSyntax.h"
#include "Trivia.h"
#include <string>

namespace nixf {

namespace tok {
enum TokenKind {
#define TOK(NAME) tok_##NAME,
#include "TokenKinds.inc"
#undef TOK
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
      : RawNode(SyntaxKind::SK_Token), Kind(Kind), Content(Content),
        LeadingTrivia(std::move(LeadingTrivia)),
        TrailingTrivia(std::move(TrailingTrivia)) {
    Length = Content.length();
    if (LeadingTrivia)
      Length += LeadingTrivia->getLength();
    if (TrailingTrivia)
      Length += TrailingTrivia->getLength();
  }

  [[nodiscard]] tok::TokenKind getKind() const { return Kind; }

  [[nodiscard]] Trivia *getLeadingTrivia() const { return LeadingTrivia.get(); }
  [[nodiscard]] Trivia *getTrailingTrivia() const {
    return TrailingTrivia.get();
  }

  [[nodiscard]] std::string_view getContent() const { return Content; }

  void dump(std::ostream &OS) const override {
    if (LeadingTrivia)
      LeadingTrivia->dump(OS);
    OS << Content;
    if (TrailingTrivia)
      TrailingTrivia->dump(OS);
  }
};

} // namespace nixf
