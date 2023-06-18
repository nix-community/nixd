#include "Parser.tab.h"

#include "nixd/Diagnostic.h"

#include "nixutil.h"

#include <gtest/gtest.h>

#include <nix/canon-path.hh>
#include <nix/eval.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <nixexpr.hh>

#include <iostream>
namespace nixd {
class DiagnosticTest {
  InitNix I;

public:
  std::unique_ptr<nix::EvalState> getDummyStore() {
    return std::make_unique<nix::EvalState>(nix::Strings{},
                                            nix::openStore("dummy://"));
  }
};

static const char *ParseErrorNix =
    R"(
{
  a = 1;
  foo

)";

TEST(Diagnostic, ConstructFromParseError) {
  DiagnosticTest T;
  auto State = T.getDummyStore();
  try {
    parse(ParseErrorNix, nix::CanonPath("/"), nix::CanonPath("/"), *State);
  } catch (const nix::ParseError &PE) {
    auto Diagnostics = mkDiagnostics(PE);
    ASSERT_TRUE(!Diagnostics.empty());
  }
}
} // namespace nixd
