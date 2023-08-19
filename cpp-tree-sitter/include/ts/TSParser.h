#pragma once

#include "tree_sitter/api.h"

namespace ts {

class TSParser {
  ::TSParser *Self;

public:
  TSParser() { Self = ts_parser_new(); }
  ~TSParser() { ts_parser_delete(Self); }

  ::TSParser *unwrap() { return Self; }
};

} // namespace ts
