#pragma once

#include "Parser.tab.h"

namespace nixd {

using namespace nix;

struct RangeIdx {
  PosIdx Begin;
  PosIdx End;

  RangeIdx(PosIdx Begin, PosIdx End) : Begin(Begin), End(End) {}
  RangeIdx(PosIdx Begin, const decltype(ParseData::end) &End)
      : RangeIdx(Begin, End.at(Begin)) {}
  RangeIdx(const void *Ptr, const decltype(ParseData::locations) &Loc,
           const decltype(ParseData::end) &End)
      : RangeIdx(Loc.at(Ptr), End) {}
};

struct Range {
  Pos Begin;
  Pos End;
  Range(Pos Begin, Pos End) : Begin(std::move(Begin)), End(std::move(End)) {}
  Range(RangeIdx Idx, const PosTable &Table) {
    Begin = Table[Idx.Begin];
    End = Table[Idx.End];
  }
  Range(PosIdx Begin, const decltype(ParseData::end) &End,
        const PosTable &Table)
      : Range(RangeIdx(Begin, End), Table) {}
  Range(const void *Ptr, const decltype(ParseData::locations) &Loc,
        const decltype(ParseData::end) &End, const PosTable &Table)
      : Range(RangeIdx(Ptr, Loc, End), Table) {}
};

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
