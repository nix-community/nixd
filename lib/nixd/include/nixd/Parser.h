#pragma once

#include "Parser.tab.h"

namespace nixd {

using namespace nix;

std::unique_ptr<ParseData> parse(char *Text, size_t Length, Pos::Origin Origin,
                                 const SourcePath &BasePath, EvalState &State,
                                 std::shared_ptr<StaticEnv> Env);

inline std::unique_ptr<ParseData> parse(char *Text, size_t Length,
                                        Pos::Origin Origin,
                                        const SourcePath &BasePath,
                                        EvalState &State) {
  return parse(Text, Length, std::move(Origin), BasePath, State,
               State.staticBaseEnv);
}

} // namespace nixd
