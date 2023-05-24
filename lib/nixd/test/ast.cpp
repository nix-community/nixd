#include <gtest/gtest.h>

#include "lspserver/Protocol.h"

#include "nixd/AST.h"

#include "nixutil.h"

#include <nix/eval.hh>

namespace nixd {

TEST(AST, lookupPosition) {
  static const char *NixSrc = R"(let x = 1; in x)";

  InitNix INix;
  auto State = INix.getDummyState();
  auto *RawExpr = State->parseExprFromString(NixSrc, "/");
  auto AST = EvalAST(RawExpr);

  auto AssertPosition = [&](lspserver::Position Pos, std::string Val) {
    ASSERT_TRUE(AST.lookupPosition(*State, Pos) != nullptr);
    std::stringstream SStream;
    AST.lookupPosition(*State, Pos)->show(State->symbols, SStream);
    ASSERT_EQ(SStream.str(), Val);
  };

  for (auto I = 0; I < 14; I++)
    AssertPosition({0, I}, "{ x = 1; }");
  for (auto I = 14; I < 20; I++)
    AssertPosition({0, I}, "x");
}

} // namespace nixd
