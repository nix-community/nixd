#pragma once

#include "tree_sitter/api.h"

#include <cstdint>
#include <string>

namespace ts {

class TSNode {
  ::TSNode Self;

public:
  TSNode(::TSNode Self) : Self(Self) {}

  operator ::TSNode() const { return Self; }

  friend bool operator==(const TSNode &LHS, const TSNode &RHS) {
    return ts_node_eq(::TSNode(LHS), ::TSNode(RHS));
  }

  [[nodiscard]] TSNode descendant(uint32_t Start, uint32_t End) const {
    return ts_node_descendant_for_byte_range(Self, Start, End);
  }

  [[nodiscard]] TSNode descendant(::TSPoint Start, ::TSPoint End) const {
    return ts_node_descendant_for_point_range(Self, Start, End);
  }

  operator std::string() {
    auto *Data = ts_node_string(Self);
    std::string Ret = Data;
    free(Data);
    return Ret;
  }
};

} // namespace ts
