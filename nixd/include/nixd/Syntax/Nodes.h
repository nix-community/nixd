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
  [[nodiscard]] virtual NodeKind getKind() const = 0;
  virtual ~Node() = default;
};

#define NODE(NAME, BODY) struct NAME : Node BODY;
#include "Nodes.inc"
#undef NODE

#define NODE(NAME, _)                                                          \
  inline Node::NodeKind NAME::getKind() const { return NK_##NAME; }
#include "Nodes.inc"
#undef NODE

} // namespace nixd::syntax
