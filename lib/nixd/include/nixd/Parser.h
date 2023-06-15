#pragma once

#include "Parser.tab.h"

namespace nixd {

using namespace nix;

std::unique_ptr<ParseData> parse(char *text, size_t length, Pos::Origin origin,
                                 const SourcePath &basePath, EvalState &state);

} // namespace nixd
