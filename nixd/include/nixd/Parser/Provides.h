#pragma once

#include "Parser.tab.h"

#include <nix/nixexpr.hh>

namespace nixd {

static inline nix::PosIdx makeCurPos(const YYLTYPE &loc, ParseData *data) {
  auto Res =
      data->state.positions.add(data->origin, loc.first_line, loc.first_column);
  data->end[Res] =
      data->state.positions.add(data->origin, loc.last_line, loc.last_column);
  return Res;
}

} // namespace nixd
