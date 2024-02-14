#include "Serialize.h"

#include <nixbc/Serialize.h>

namespace nixf {

using nixbc::serialize;
using nixbc::serializeNodeMeta;

void serialize(std::ostream &OS, const ExprInt &N) {
  serializeNodeMeta(OS, nixbc::EK_Int, reinterpret_cast<std::uintptr_t>(&N));
  serialize(OS, N.value());
}

void serialize(std::ostream &OS, const ExprBinOp &N) {
  using namespace tok;

  if (!N.lhs() || !N.rhs()) // Skip invalid nodes
    return;

  const Expr &LHS = *N.lhs();
  const Expr &RHS = *N.rhs();

  switch (N.op().op()) {
  case tok_op_add:
    // (+ a b) -> (ConcatStrings a b)
    serializeNodeMeta(OS, nixbc::EK_ConcatStrings,
                      reinterpret_cast<std::uintptr_t>(&N));
    serialize(OS, LHS);
    serialize(OS, RHS);
    return;
  default:
    break;
  }
  assert(false && "Unhandled binary operator");
  __builtin_unreachable();
}

void serialize(std::ostream &OS, const Node &N) {
  switch (N.kind()) {
  case Node::NK_ExprInt:
    serialize(OS, static_cast<const ExprInt &>(N));
    break;
  case Node::NK_ExprBinOp:
    serialize(OS, static_cast<const ExprBinOp &>(N));
    break;
  default:
    break;
  }
}

} // namespace nixf
