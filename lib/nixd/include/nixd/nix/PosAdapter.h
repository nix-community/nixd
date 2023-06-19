#pragma once

#include <nix/nixexpr.hh>

namespace nix {
struct PosAdapter : AbstractPos {
  Pos::Origin origin;
};
} // namespace nix
