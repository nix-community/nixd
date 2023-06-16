#pragma once

#include "nixd/Parser.h"

#include "lspserver/Protocol.h"

#include <nix/nixexpr.hh>

namespace nixd {

inline lspserver::Position toLSPPos(const nix::AbstractPos &P) {
  return {.line = static_cast<int>(P.line - 1),
          .character = static_cast<int>(P.column - 1)};
}

inline lspserver::Position toLSPPos(const nix::Pos &Pos) {
  return {static_cast<int>(Pos.line - 1), static_cast<int>(Pos.column - 1)};
}

inline lspserver::Range toLSPRange(const Range &R) {
  return {toLSPPos(R.Begin), toLSPPos(R.End)};
}

}; // namespace nixd
