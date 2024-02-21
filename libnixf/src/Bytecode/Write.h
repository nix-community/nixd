#include "nixf/Bytecode/Write.h"

#include <nixbc/Nodes.h>

namespace nixf {

void writeBytecode(std::ostream &OS, const ExprInt &N);

void writeBytecode(std::ostream &OS, const ExprBinOp &N);

} // namespace nixf
