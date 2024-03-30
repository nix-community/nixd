/// \file
/// \brief Serialization of nixf nodes.
///
/// Serialization is the process of converting nix nodes into a byte stream. The
/// target format is "nixbc".

#pragma once

#include "nixf/Basic/Nodes/Basic.h"

namespace nixf {

void writeBytecode(std::ostream &OS, const Node &N);

} // namespace nixf
