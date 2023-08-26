#include <gtest/gtest.h>

#include "lspserver/Protocol.h"

#include "nixd/AST/AttrLocator.h"
#include "nixd/AST/EvalAST.h"
#include "nixd/AST/ParseAST.h"
#include "nixd/Parser/Parser.h"

#include "nixutil.h"

#include <nix/canon-path.hh>
#include <nix/eval.hh>

#include <boost/algorithm/string/join.hpp>

namespace nixd {

TEST(AST, lookupEnd) {
  std::string NixSrc = R"(
{
  a = {
    b = 1;
  };

  d = {
    z = {
      y = 1;
    };
  };
}
  )";
  InitNix INix;
  auto State = INix.getDummyState();
  auto AST = ParseAST::create(
      parse(NixSrc, nix::CanonPath("foo"), nix::CanonPath("/"), *State));
  const auto *E = AST->lookupEnd({4, 4});
  if (const auto *EA = dynamic_cast<const nix::ExprAttrs *>(E)) {
    auto Pos = State->positions[AST->getPos(EA)];
    ASSERT_EQ(Pos.line, 3);
  } else {
    ASSERT_TRUE(false && "It must be an Attrs!");
  }
}

TEST(AST, LocationContext) {
  std::string NixSrc = R"(
{
  a = {
# ^AttrName
    b = 1;
#       ^Value
  };


# ^Unknown

  d = {
    z = {
      y = 1;
    };
  };

  list = [  ];
#          ^Value
}
  )";
  InitNix INix;
  auto State = INix.getDummyState();
  auto AST = ParseAST::create(
      parse(NixSrc, nix::CanonPath("foo"), nix::CanonPath("/"), *State));
  ASSERT_EQ(AST->getContext({2, 2}), ParseAST::LocationContext::AttrName);
  ASSERT_EQ(AST->getContext({4, 8}), ParseAST::LocationContext::Value);
  ASSERT_EQ(AST->getContext({8, 8}), ParseAST::LocationContext::Unknown);
  ASSERT_EQ(AST->getContext({17, 10}), ParseAST::LocationContext::Value);
}

TEST(AST, lookupContainMin) {
  std::string NixSrc = R"(
{
  a = {
    b = 1;
  };

  d = {
    z = {
      y = 1;
    };
  };
}
  )";
  InitNix INix;
  auto State = INix.getDummyState();
  auto AST = ParseAST::create(
      parse(NixSrc, nix::CanonPath("foo"), nix::CanonPath("/"), *State));
  const auto *E = AST->lookupContainMin({3, 1});
  if (const auto *EA = dynamic_cast<const nix::ExprAttrs *>(E)) {
    auto Pos = State->positions[AST->getPos(EA)];
    ASSERT_EQ(Pos.line, 3);
  } else {
    ASSERT_TRUE(false && "It must be an Attrs!");
  }
}

static void checkVec(const std::vector<std::string> &Vec,
                     const std::string &Expected) {
  auto VecStr = boost::algorithm::join(Vec, ".");
  ASSERT_EQ(VecStr, Expected);
}

TEST(AST, AttrLocator) {
  std::string NixSrc = R"(
{
  x.y = 1;
  foo.bar.baz.foo.
}
  )";
  InitNix INix;
  auto State = INix.getDummyState();
  auto AST = ParseAST::create(
      parse(NixSrc, nix::CanonPath("foo"), nix::CanonPath("/"), *State));
  AttrLocator Locator(*AST);
  checkVec(AST->getAttrPath(Locator.locate({2, 4})), "x.y");
  checkVec(AST->getAttrPath(Locator.locate({3, 16})), "foo.bar.baz.foo");
}

} // namespace nixd
