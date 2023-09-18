#pragma once

#include "Parser/Require.h"

namespace nixd::syntax {
void parse(char *Text, size_t Size, ParseData *Data);
inline void parse(std::string Text, ParseData *Data) {
  Text.append("\0\0", 2);
  parse(Text.data(), Text.length(), Data);
}
} // namespace nixd::syntax
