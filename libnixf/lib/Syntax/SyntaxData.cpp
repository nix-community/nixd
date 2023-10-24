#include "nixf/Syntax/SyntaxData.h"

#include <cassert>

namespace nixf {

SyntaxData::SyntaxData(std::size_t Offset, std::shared_ptr<RawNode> RN,
                       const SyntaxData *Parent)
    : Offset(Offset), Raw(std::move(RN)), Parent(Parent) {
  assert(Raw);
}

[[nodiscard]] std::size_t SyntaxData::getNumChildren() const {
  return Raw->getNumChildren();
}

std::shared_ptr<SyntaxData> SyntaxData::getNthChild(std::size_t N) const {
  if (N > getNumChildren())
    return nullptr;
  // Compute the offset of this child.
  std::size_t ChOffset = Offset;
  for (std::size_t I = 0; I < N - 1; I++) {
    ChOffset += Raw->getNthChild(I)->getLength();
  }
  return std::make_shared<SyntaxData>(ChOffset, Raw->getNthChild(N), this);
}

} // namespace nixf
