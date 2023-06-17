#pragma once

#include "nixd/Parser.h"

#include "lspserver/Protocol.h"

#include <nix/nixexpr.hh>

namespace nixd {

struct Range;

inline lspserver::Range toLSPRange(const Range &R);

struct RangeIdx {
  PosIdx Begin;
  PosIdx End;

  RangeIdx(PosIdx Begin, PosIdx End) : Begin(Begin), End(End) {}
  RangeIdx(PosIdx Begin, const decltype(ParseData::end) &End)
      : RangeIdx(Begin, End.at(Begin)) {}
  RangeIdx(const void *Ptr, const decltype(ParseData::locations) &Loc,
           const decltype(ParseData::end) &End)
      : RangeIdx(Loc.at(Ptr), End) {}
};

struct Range {
  Pos Begin;
  Pos End;
  Range(Pos Begin, Pos End) : Begin(std::move(Begin)), End(std::move(End)) {}
  Range(RangeIdx Idx, const PosTable &Table) {
    Begin = Table[Idx.Begin];
    End = Table[Idx.End];
  }
  Range(PosIdx Begin, const decltype(ParseData::end) &End,
        const PosTable &Table)
      : Range(RangeIdx(Begin, End), Table) {}
  Range(const void *Ptr, const decltype(ParseData::locations) &Loc,
        const decltype(ParseData::end) &End, const PosTable &Table)
      : Range(RangeIdx(Ptr, Loc, End), Table) {}

  operator lspserver::Range() const { return toLSPRange(*this); }
};

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
