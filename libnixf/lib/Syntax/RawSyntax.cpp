#include "nixf/Syntax/RawSyntax.h"
#include "nixf/Syntax/Syntax.h"

namespace nixf {

RawTwine::RawTwine(SyntaxKind Kind,
                   std::vector<std::unique_ptr<RawNode>> Layout)
    : RawNode(Kind), Layout(std::move(Layout)) {
  Length = 0;
  for (const auto &Ch : this->Layout) {
    if (Ch)
      Length += Ch->getLength();
  }
}

void RawTwine::dump(std::ostream &OS, bool DiscardTrivia) const {
  for (const auto &Node : Layout) {
    if (Node)
      Node->dump(OS, DiscardTrivia);
  }
};

[[nodiscard]] RawNode *RawTwine::getNthChild(std::size_t N) const {
  if (N > getNumChildren())
    return nullptr;
  return Layout[N].get();
}

} // namespace nixf
