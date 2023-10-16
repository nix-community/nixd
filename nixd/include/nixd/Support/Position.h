#pragma once

#include "nixd/Basic/Range.h"
#include "nixd/Nix/PosAdapter.h"

#include "lspserver/Protocol.h"

#include <nix/nixexpr.hh>

namespace nixd {

struct Range;

inline lspserver::Range toLSPRange(const Range &R);

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
