#pragma once

#include "Parser.tab.h"

#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>

#include <filesystem>
#include <utility>

namespace nixd {

using ParseSession =
    std::tuple<nix::SymbolTable, nix::PosTable, std::unique_ptr<ParseData>>;

std::unique_ptr<ParseData> parse(char *Text, size_t Length,
                                 nix::Pos::Origin Origin,
                                 const nix::SourcePath &BasePath,
                                 ParseState State);

inline std::unique_ptr<ParseData> parse(std::string Text,
                                        nix::Pos::Origin Origin,
                                        const nix::SourcePath &BasePath,
                                        ParseState State) {
  Text.append("\0\0", 2);
  return parse(Text.data(), Text.length(), std::move(Origin), BasePath, State);
}

inline ParseSession parse(char *Text, size_t Length, nix::Pos::Origin Origin,
                          const nix::SourcePath &BasePath) {
  nix::SymbolTable Symbols;
  nix::PosTable Positions;
  ParseState State{Symbols, Positions};
  auto Data = parse(Text, Length, std::move(Origin), BasePath, State);
  return {std::move(Symbols), std::move(Positions), std::move(Data)};
}

inline ParseSession parse(std::string Text, nix::Pos::Origin Origin,
                          const nix::SourcePath &BasePath) {
  Text.append("\0\0", 2);
  return parse(Text.data(), Text.length(), std::move(Origin), BasePath);
}

inline ParseSession parse(std::string Text, const std::string &Path) {
  Text.append("\0\0", 2);
  auto FSPath = std::filesystem::path(Path);
  auto Origin = nix::CanonPath(FSPath.string());
  auto BasePath = nix::CanonPath(FSPath.remove_filename().string());
  return parse(Text.data(), Text.length(), std::move(Origin),
               std::move(BasePath));
}

inline std::unique_ptr<ParseData> parse(char *Text, size_t Length,
                                        nix::Pos::Origin Origin,
                                        const nix::SourcePath &BasePath,
                                        nix::EvalState &State,
                                        std::shared_ptr<nix::StaticEnv> Env) {
  auto Data = parse(Text, Length, std::move(Origin), BasePath,
                    ParseState{State.symbols, State.positions});
  if (Data->result && Data->error.empty())
    Data->result->bindVars(State, Env);
  return Data;
}

inline std::unique_ptr<ParseData> parse(char *Text, size_t Length,
                                        nix::Pos::Origin Origin,
                                        const nix::SourcePath &BasePath,
                                        nix::EvalState &State) {
  return parse(Text, Length, std::move(Origin), BasePath, State,
               State.staticBaseEnv);
}

inline std::unique_ptr<ParseData> parse(std::string Text,
                                        nix::Pos::Origin Origin,
                                        const nix::SourcePath &BasePath,
                                        nix::EvalState &State,
                                        std::shared_ptr<nix::StaticEnv> Env) {
  Text.append("\0\0", 2);
  return parse(Text.data(), Text.length(), std::move(Origin), BasePath, State,
               std::move(Env));
}

inline std::unique_ptr<ParseData> parse(std::string Text,
                                        nix::Pos::Origin Origin,
                                        const nix::SourcePath &BasePath,
                                        nix::EvalState &State) {
  Text.append("\0\0", 2);
  return parse(Text.data(), Text.length(), std::move(Origin), BasePath, State);
}

} // namespace nixd
