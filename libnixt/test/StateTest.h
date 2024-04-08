#include <gtest/gtest.h>

#include <nix/eval.hh>
#include <nix/store-api.hh>

namespace nixt {

struct StateTest : testing::Test {
  std::unique_ptr<nix::EvalState> State;
  StateTest() : State(new nix::EvalState{{}, nix::openStore("dummy://")}) {}
};

} // namespace nixt
