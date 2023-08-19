#pragma once

#include "tree_sitter/api.h"

namespace ts {

class TSTreePtr {
  ::TSTree *Self;

public:
  TSTreePtr(::TSTree *Self) : Self(Self) {}
  ~TSTreePtr() { ts_tree_delete(Self); }

  operator ::TSTree *() const { return Self; }

  ::TSNode rootNode() { return ts_tree_root_node(Self); }
};

} // namespace ts
