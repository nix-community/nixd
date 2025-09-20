#include <gtest/gtest.h>

#include <nix/cmd/common-eval-args.hh>
#include <nix/expr/eval.hh>
#include <nix/store/store-open.hh>

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
