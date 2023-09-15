#pragma once

#include "Parser/Require.h"

namespace nixd::syntax {
void parse(char *Text, size_t Size, ParseData *Data);
} // namespace nixd::syntax
