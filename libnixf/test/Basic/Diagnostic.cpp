#include <gtest/gtest.h>

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Range.h"

namespace {

using namespace nixf;

TEST(Diagostic, Format) {
  Diagnostic Diag(Diagnostic::DK_UnexpectedBetween, LexerCursorRange{});
  Diag << "foo"
       << "bar"
       << "baz";
  ASSERT_EQ(Diag.format(), "unexpected foo between bar and baz");
}

} // namespace
