#pragma once

#include "nixd/Nix/PosAdapter.h"
#include "nixd/Parser/Parser.h"

#include "lspserver/Protocol.h"

#include <nix/nixexpr.hh>

namespace nixd {

struct Range;

inline lspserver::Range toLSPRange(const Range &R);

struct RangeIdx {
  nix::PosIdx Begin;
  nix::PosIdx End;

  RangeIdx(nix::PosIdx Begin, nix::PosIdx End) : Begin(Begin), End(End) {}
  RangeIdx(nix::PosIdx Begin, const decltype(ParseData::end) &End)
      : RangeIdx(Begin, End.at(Begin)) {}
  RangeIdx(const void *Ptr, const decltype(ParseData::locations) &Loc,
           const decltype(ParseData::end) &End)
      : RangeIdx(Loc.at(Ptr), End) {}
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
  Range(nix::PosIdx Begin, const decltype(ParseData::end) &End,
        const nix::PosTable &Table)
      : Range(RangeIdx(Begin, End), Table) {}
  Range(const void *Ptr, const decltype(ParseData::locations) &Loc,
        const decltype(ParseData::end) &End, const nix::PosTable &Table)
      : Range(RangeIdx(Ptr, Loc, End), Table) {}

  operator lspserver::Range() const { return toLSPRange(*this); }
};

inline lspserver::Position toLSPPos(const nix::AbstractPos &P) {
  return {static_cast<int>(std::max(1U, P.line) - 1),
          static_cast<int>(std::max(1U, P.column) - 1)};
}

inline lspserver::Position toLSPPos(const nix::Pos &P) {
  return {static_cast<int>(std::max(1U, P.line) - 1),
          static_cast<int>(std::max(1U, P.column) - 1)};
}

inline lspserver::Range toLSPRange(const Range &R) {
  return {toLSPPos(R.Begin), toLSPPos(R.End)};
}

inline std::string pathOf(const nix::PosAdapter &Pos) {
  if (const auto &Path = std::get_if<nix::SourcePath>(&Pos.origin)) {
    return Path->to_string();
  }
  return "/position-is-not-path";
}

inline std::string pathOf(const nix::AbstractPos *Pos) {
  if (const auto *PAPos = dynamic_cast<const nix::PosAdapter *>(Pos))
    return pathOf(*PAPos);
  return "/nix-abstract-position";
}

}; // namespace nixd
