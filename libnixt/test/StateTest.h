#include <gtest/gtest.h>

#include <nix/common-eval-args.hh>
#include <nix/eval.hh>
#include <nix/store-api.hh>

namespace nixt {

struct StateTest : testing::Test {
  std::unique_ptr<nix::EvalState> State;
  StateTest()
      : State(new nix::EvalState{{},
                                 nix::openStore("dummy://"),
                                 nix::fetchSettings,
                                 nix::evalSettings}) {}
};

} // namespace nixt
