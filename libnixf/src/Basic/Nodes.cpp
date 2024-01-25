#include "nixf/Basic/Nodes.h"

namespace nixf {

[[nodiscard]] const char *Node::name(NodeKind Kind) {
  switch (Kind) {
#define EXPR(NAME)                                                             \
  case NK_##NAME:                                                              \
    return #NAME;
#define NODE(NAME)                                                             \
  case NK_##NAME:                                                              \
    return #NAME;
#include "nixf/Basic/NodeKinds.inc"
#undef EXPR
#undef NODE
  default:
    assert(false && "Not yet implemented!");
  }
  assert(false && "Not yet implemented!");
  __builtin_unreachable();
}

InterpolatedParts::InterpolatedParts(LexerCursorRange Range,
                                     std::vector<InterpolablePart> Fragments)
    : Node(NK_InterpolableParts, Range), Fragments(std::move(Fragments)) {
  // Check if the fragment forms a string literal (i.e. no interpolation)
  for (const InterpolablePart &Frag : this->Fragments) {
    if (Frag.kind() == InterpolablePart::SPK_Interpolation)
      return;
  }

  // Concatenate the fragments into a singe "Escaped"
  std::string Escaped;
  for (const InterpolablePart &Frag : this->Fragments) {
    assert(Frag.kind() == InterpolablePart::SPK_Escaped &&
           "Only Escaped fragments can be concatenated");
    Escaped += Frag.escaped();
  }
  Fragments.clear();
  Fragments.emplace_back(std::move(Escaped));
}

} // namespace nixf
