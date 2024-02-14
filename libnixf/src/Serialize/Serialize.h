#include "nixf/Serialize/Serialize.h"

#include <nixbc/Nodes.h>

namespace nixf {

void serialize(std::ostream &OS, const ExprInt &N);

void serialize(std::ostream &OS, const ExprBinOp &N);

} // namespace nixf
