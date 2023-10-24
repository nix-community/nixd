#include "nixf/Syntax/RawSyntax.h"
#include "nixf/Syntax/Syntax.h"

namespace nixf {

RawTwine::RawTwine(SyntaxKind Kind,
                   std::vector<std::shared_ptr<RawNode>> Layout)
    : Layout(std::move(Layout)), Kind(Kind) {
  for (const auto &Ch : Layout) {
    Length = 0;
    if (Ch)
      Length += Ch->getLength();
  }
}

void RawTwine::dump(std::ostream &OS) const {
  for (const auto &Node : Layout) {
    Node->dump(OS);
  }
};

[[nodiscard]] std::shared_ptr<RawNode>
RawTwine::getNthChild(std::size_t N) const {
  if (N > getNumChildren())
    return nullptr;
  return Layout[N];
}

} // namespace nixf
