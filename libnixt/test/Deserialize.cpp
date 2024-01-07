#include <gtest/gtest.h>

#include "nixt/ArrayRef.h"
#include "nixt/PtrPool.h"
#include "nixt/Serialize.h"

using namespace nixt::serialize;
using namespace nixt;
using namespace std::literals;

namespace {

class DeserializeTest : public testing::Test {
protected:
  nix::PosTable PT;
  nix::SymbolTable ST;
  std::ostringstream OS;
  std::string Result;
  PtrPool<nix::Expr> Pool;

  BytesRef encode(const nix::Expr *E) {
    nixt::serialize::encodeAST(OS, ST, PT, nix::Pos::none_tag{}, E);
    Result = OS.str();
    return {Result.data(), Result.data() + Result.size()};
  }
  nix::PosIdx somePos() {
    nix::Pos::Origin O = nix::Pos::none_tag{};
    return PT.add(O, 32, 32);
  }
  nix::AttrName someAttrNameSymbol() { return {ST.create("hello")}; }
  static nix::AttrName someAttrNameExpr() { return nullptr; }
};

std::unique_ptr<nix::ExprInt> mkInt(nix::NixInt N) {
  return std::make_unique<nix::ExprInt>(N);
}

template <class T> void check(BytesRef &Data, T Expected) {
  EXPECT_EQ(consume<T>(Data), Expected);
}

void checkEmpty(BytesRef &Data) { EXPECT_EQ(lengthof(Data), 0); }

TEST_F(DeserializeTest, ExprInt) {
  auto Data = encode(mkInt(0xdeadbeef).get());
  auto *E = consumeAST(Data, Pool, PT, ST);
  EXPECT_EQ(static_cast<nix::ExprInt *>(E)->n, 0xdeadbeef);
  checkEmpty(Data);
}

TEST_F(DeserializeTest, ExprFloat) {
  auto Data = encode(new nix::ExprFloat{3.14});
  auto *E = consumeAST(Data, Pool, PT, ST);
  EXPECT_EQ(static_cast<nix::ExprFloat *>(E)->nf, 3.14);
  checkEmpty(Data);
}

TEST_F(DeserializeTest, ExprString) {
  auto Data = encode(new nix::ExprString{"hello"});
  auto *E = consumeAST(Data, Pool, PT, ST);
  EXPECT_EQ(static_cast<nix::ExprString *>(E)->s, "hello"s);
  checkEmpty(Data);
}

TEST_F(DeserializeTest, ExprPath) {
  auto Data = encode(new nix::ExprPath{"hello"});
  auto *E = consumeAST(Data, Pool, PT, ST);
  EXPECT_EQ(static_cast<nix::ExprPath *>(E)->s, "hello"s);
  checkEmpty(Data);
}

TEST_F(DeserializeTest, ExprVar) {
  auto EV = std::make_unique<nix::ExprVar>(somePos(), ST.create("hello"));
  EV->level = 43;
  EV->displ = 42;
  EV->fromWith = true;
  auto Data = encode(EV.get());
  auto *E = static_cast<nix::ExprVar *>(consumeAST(Data, Pool, PT, ST));
  EXPECT_EQ(E->level, 43);
  EXPECT_EQ(E->displ, 42);
  EXPECT_EQ(E->fromWith, true);
  EXPECT_EQ(E->name, ST.create("hello"));
  checkEmpty(Data);
}

} // namespace
