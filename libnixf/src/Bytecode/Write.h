#include "nixf/Basic/Nodes/Op.h"
#include "nixf/Basic/Nodes/Simple.h"

#include <nixbc/Nodes.h>

#include <ostream>

namespace nixf {

void writeBytecode(std::ostream &OS, const ExprInt &N);

void writeBytecode(std::ostream &OS, const ExprBinOp &N);

} // namespace nixf
