#include "Write.h"

#include "nixf/Basic/Nodes/Op.h"
#include "nixf/Bytecode/Write.h"

#include <bc/Write.h>

#include <nixbc/Nodes.h>

namespace nixf {

using namespace nixbc;

using bc::writeBytecode;

void writeBytecode(std::ostream &OS, const ExprInt &N) {
  writeBytecode(OS, NodeHeader{EK_Int, reinterpret_cast<std::uintptr_t>(&N)});
  writeBytecode(OS, N.value());
}

void writeBytecode(std::ostream &OS, const ExprBinOp &N) {
  using namespace tok;

  if (!N.lhs() || !N.rhs()) // Skip invalid nodes
    return;

  const Expr &LHS = *N.lhs();
  const Expr &RHS = *N.rhs();

  switch (N.op().op()) {
  case tok_op_add:
    // (+ a b) -> (ConcatStrings a b)
    writeBytecode(
        OS, NodeHeader{EK_ConcatStrings, reinterpret_cast<std::uintptr_t>(&N)});
    writeBytecode(OS, static_cast<const Node &>(LHS));
    writeBytecode(OS, static_cast<const Node &>(RHS));
    return;
  default:
    break;
  }
  assert(false && "Unhandled binary operator");
  __builtin_unreachable();
}

void writeBytecode(std::ostream &OS, const Node &N) {
  switch (N.kind()) {
  case Node::NK_ExprInt:
    writeBytecode(OS, static_cast<const ExprInt &>(N));
    break;
  case Node::NK_ExprBinOp:
    writeBytecode(OS, static_cast<const ExprBinOp &>(N));
    break;
  default:
    break;
  }
}

} // namespace nixf
