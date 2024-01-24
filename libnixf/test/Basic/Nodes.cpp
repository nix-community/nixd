#include <gtest/gtest.h>

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/Nodes.h"
#include "nixf/Parse/Parser.h"

namespace {

using namespace std::literals;
using namespace nixf;

TEST(Node, Descend) {
  auto Src = "{ a = 1; }"sv;
  std::vector<Diagnostic> Diag;
  auto Root = parse(Src, Diag);

  ASSERT_EQ(Root->descend({{0, 2}, {0, 2}})->kind(), Node::NK_Identifer);
  ASSERT_EQ(Root->descend({{0, 2}, {0, 4}})->kind(), Node::NK_Binding);
}

} // namespace
