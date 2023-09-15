/// FIXME: comment for this file.
#pragma once

#include "Range.h"

namespace nixd::syntax {

/// Syntax nodes
/// TODO: the comment
struct Node {
  nixd::RangeIdx Range;
  enum NodeKind {
    NK_Node,
#define NODE(NAME, BODY) NK_##NAME,
#include "Nodes.inc"
#undef NODE
  };
  virtual NodeKind getKind() { return NK_Node; };
  virtual ~Node() = default;
};

#define COMMON_METHOD virtual NodeKind getKind() override;

#define NODE(NAME, BODY) struct NAME : Node BODY;
#include "Nodes.inc"
#undef NODE
#undef COMMON_METHOD

#define NODE(NAME, _)                                                          \
  inline Node::NodeKind NAME::getKind() { return NK_##NAME; }
#include "Nodes.inc"
#undef NODE

} // namespace nixd::syntax
