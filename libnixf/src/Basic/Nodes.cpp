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

} // namespace nixf
