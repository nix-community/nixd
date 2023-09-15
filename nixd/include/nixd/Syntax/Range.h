#pragma once

#include <nix/nixexpr.hh>

namespace nixd {

struct RangeIdx {
  nix::PosIdx Begin;
  nix::PosIdx End;
};

struct Range {
  nix::Pos Begin;
  nix::Pos End;
  Range(nix::Pos Begin, nix::Pos End)
      : Begin(std::move(Begin)), End(std::move(End)) {}
  Range(RangeIdx Idx, const nix::PosTable &Table) {
    Begin = Table[Idx.Begin];
    End = Table[Idx.End];
  }
};

} // namespace nixd
