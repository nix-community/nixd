/// \file
/// \brief Serialization of nixf nodes.
///
/// Serialization is the process of converting nix nodes into a byte stream. The
/// target format is "nixbc".

#pragma once

#include "nixf/Basic/Nodes.h"

#include <cstdint>

namespace nixf {

void serialize(std::ostream &OS, const Node &N);

} // namespace nixf
