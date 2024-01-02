#include <gtest/gtest.h>

#include "nixt/Kinds.h"

#include <nix/nixexpr.hh>

namespace {

using namespace nixt;
using namespace nixt::ek;

TEST(Kinds, kindOf) {
  nix::ExprInt EInt(42);
  EXPECT_EQ(kindOf(EInt), EK_ExprInt);
}

TEST(Kinds, nameOf) { EXPECT_STREQ(nameOf(EK_ExprInt), "ExprInt"); }

} // namespace
