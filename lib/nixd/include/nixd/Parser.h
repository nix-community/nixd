#pragma once

#include "Parser.tab.h"
#include "nixexpr.hh"
#include "symbol-table.hh"

#include <utility>

namespace nixd {

using namespace nix;

std::unique_ptr<ParseData> parse(char *Text, size_t Length, Pos::Origin Origin,
                                 const SourcePath &BasePath, ParseState State);

inline std::unique_ptr<ParseData> parse(std::string Text, Pos::Origin Origin,
                                        const SourcePath &BasePath,
                                        ParseState State) {
  Text.append("\0\0", 2);
  return parse(Text.data(), Text.length(), std::move(Origin), BasePath, State);
}

inline std::tuple<nix::SymbolTable, nix::PosTable, std::unique_ptr<ParseData>>
parse(char *Text, size_t Length, Pos::Origin Origin,
      const SourcePath &BasePath) {
  nix::SymbolTable Symbols;
  nix::PosTable Positions;
  ParseState State{Symbols, Positions};
  auto Data = parse(Text, Length, std::move(Origin), BasePath, State);
  return {std::move(Symbols), std::move(Positions), std::move(Data)};
}

inline std::tuple<nix::SymbolTable, nix::PosTable, std::unique_ptr<ParseData>>
parse(std::string Text, Pos::Origin Origin, const SourcePath &BasePath) {
  Text.append("\0\0", 2);
  return parse(Text.data(), Text.length(), std::move(Origin), BasePath);
}

inline std::unique_ptr<ParseData>
parse(char *Text, size_t Length, Pos::Origin Origin, const SourcePath &BasePath,
      EvalState &State, std::shared_ptr<StaticEnv> Env) {
  auto Data = parse(Text, Length, std::move(Origin), BasePath,
                    ParseState{State.symbols, State.positions});
  if (Data->result && Data->error.empty())
    Data->result->bindVars(State, Env);
  return Data;
}

inline std::unique_ptr<ParseData> parse(char *Text, size_t Length,
                                        Pos::Origin Origin,
                                        const SourcePath &BasePath,
                                        EvalState &State) {
  return parse(Text, Length, std::move(Origin), BasePath, State,
               State.staticBaseEnv);
}

inline std::unique_ptr<ParseData> parse(std::string Text, Pos::Origin Origin,
                                        const SourcePath &BasePath,
                                        EvalState &State,
                                        std::shared_ptr<StaticEnv> Env) {
  Text.append("\0\0", 2);
  return parse(Text.data(), Text.length(), std::move(Origin), BasePath, State,
               std::move(Env));
}

inline std::unique_ptr<ParseData> parse(std::string Text, Pos::Origin Origin,
                                        const SourcePath &BasePath,
                                        EvalState &State) {
  Text.append("\0\0", 2);
  return parse(Text.data(), Text.length(), std::move(Origin), BasePath, State);
}

} // namespace nixd
