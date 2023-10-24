#include "nixf/Syntax/Syntax.h"
#include "nixf/Syntax/RawSyntax.h"
#include "nixf/Syntax/SyntaxData.h"
#include <cassert>

namespace nixf {

Syntax::Syntax(std::shared_ptr<SyntaxData> Root, const SyntaxData *Data)
    : Root(std::move(Root)), Data(Data) {
  assert(this->Data);
  assert(this->Root);
}

std::optional<Syntax> Syntax::getParent() const {
  if (Data->getParent())
    return Syntax{Root, Data->getParent()};
  return std::nullopt;
}

std::shared_ptr<RawNode> Syntax::getRaw() const { return Data->getRaw(); }

SyntaxKind Syntax::getSyntaxKind() const { return Data->getSyntaxKind(); }

std::shared_ptr<SyntaxData> FormalSyntax::getName() const {
  return Data->getNthChild(0);
}

} // namespace nixf
