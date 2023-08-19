#pragma once

#include "TSTreePtr.h"

#include "tree_sitter/api.h"

#include <string>

namespace ts {

class TSParserPtr {
  ::TSParser *Self;

public:
  TSParserPtr() { Self = ts_parser_new(); }
  TSParserPtr(::TSParser *Self) : Self(Self) {}
  ~TSParserPtr() { ts_parser_delete(Self); }

  operator ::TSParser *() const { return Self; }

  bool setLanguage(const TSLanguage *Language) {
    return ts_parser_set_language(Self, Language);
  }

  TSTreePtr parse(const TSTreePtr &OldTree, TSInput Input) {
    return ts_parser_parse(Self, OldTree, Input);
  }

  TSTreePtr parse(const TSTreePtr &OldTree, std::string Str) {
    return ts_parser_parse_string(Self, OldTree, Str.data(), Str.length());
  }
};

} // namespace ts
