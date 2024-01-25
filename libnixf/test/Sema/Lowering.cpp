#include <gtest/gtest.h>

#include "nixf/Parse/Parser.h"
#include "nixf/Sema/Lowering.h"

namespace {

using namespace nixf;
using namespace std::literals;

class Lowering : public ::testing::Test {
protected:
  std::vector<Diagnostic> Diags;
};

TEST_F(Lowering, dupAttr) {
  auto Src = R"(
{
  a.a = 1;
  a.a.b = 2;
}
    )"sv;

  auto AST = nixf::parse(Src, Diags);
  lower(AST.get(), Diags);
}

} // namespace
