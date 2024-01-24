#include "nixf/Basic/Nodes.h"

namespace nixf {

[[nodiscard]] const char *Node::name(NodeKind Kind) {
  switch (Kind) {
  case NK_InterpolableParts:
    return "InterpolableParts";
  case NK_Misc:
    return "Misc";
  case NK_Identifer:
    return "Identifer";
  case NK_AttrName:
    return "AttrName";
  case NK_AttrPath:
    return "AttrPath";
  case NK_Binding:
    return "Binding";
  case NK_Binds:
    return "Binds";
  case NK_ExprInt:
    return "ExprInt";
  case NK_ExprFloat:
    return "ExprFloat";
  case NK_ExprVar:
    return "ExprVar";
  case NK_ExprString:
    return "ExprString";
  case NK_ExprPath:
    return "ExprPath";
  case NK_ExprParen:
    return "ExprParen";
  case NK_ExprAttrs:
    return "ExprAttrs";
  default:
    assert(false && "Not yet implemented!");
  }
}

} // namespace nixf
