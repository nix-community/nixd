#include <gtest/gtest.h>

#include "nixt/ArrayRef.h"
#include "nixt/Serialize.h"

using namespace nixt::serialize;
using namespace nixt;
using namespace std::literals;

namespace {

class SerializeTest : public testing::Test {
protected:
  nix::PosTable PT;
  nix::SymbolTable ST;
  std::ostringstream OS;
  std::string Result;

  template <class T>
    requires(!std::is_pointer_v<T>)
  BytesRef encode(const T &Obj) {
    nixt::serialize::encode(OS, Obj);
    Result = OS.str();
    return {Result.data(), Result.data() + Result.size()};
  }

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

void checkASTHeader(BytesRef &Data) {
  auto Header = consume<ASTHeader>(Data);
  EXPECT_EQ(std::memcmp(Header.Magic, "\x7FNixAST\0", 8), 0);
  EXPECT_EQ(Header.Version, 1);
}

void checkTrivialOrigin(BytesRef &Data) {
  EXPECT_EQ(consume<std::size_t>(Data), 0);
}

template <class T> void check(BytesRef &Data, T Expected) {
  EXPECT_EQ(consume<T>(Data), Expected);
}

void checkExprInt(BytesRef &Data, nix::NixInt N) {
  check(Data, EncodeKind::ExprInt);
  check(Data, N);
}

void checkEmpty(BytesRef &Data) { EXPECT_EQ(lengthof(Data), 0); }

void checkSomePos(BytesRef &Data) {
  check(Data, 32);
  check(Data, 32);
}

TEST_F(SerializeTest, Origin_none_tag) {
  auto Data = encode(nix::Pos::Origin{nix::Pos::none_tag{}});
  check(Data, (std::size_t)0);
  checkEmpty(Data);
}

TEST_F(SerializeTest, Origin_Stdin) {
  auto Data = encode(nix::Pos::Stdin{nix::make_ref<std::string>("")});
  check(Data, (std::size_t)1);
  checkEmpty(Data);
}

TEST_F(SerializeTest, Origin_String) {
  auto Data = encode(nix::Pos::String{nix::make_ref<std::string>("")});
  check(Data, (std::size_t)2);
  checkEmpty(Data);
}

TEST_F(SerializeTest, ExprInt) {
  auto Data = encode(mkInt(0xdeadbeef).get());
  checkASTHeader(Data);
  checkTrivialOrigin(Data);
  checkExprInt(Data, 0xdeadbeef);
  checkEmpty(Data);
}

TEST_F(SerializeTest, ExprFloat) {
  union FloatUnion {
    nix::NixFloat F;
    uint64_t Int;
  };
  FloatUnion FU;
  FU.Int = 0xdeadbeef;
  auto EF = std::make_unique<nix::ExprFloat>(FU.F);
  auto Data = encode(EF.get());
  checkASTHeader(Data);
  checkTrivialOrigin(Data);
  check(Data, EncodeKind::ExprFloat);
  check(Data, (uint64_t)0xdeadbeefUL);
  checkEmpty(Data);
}

TEST_F(SerializeTest, ExprString) {
  auto ES = std::make_unique<nix::ExprString>("hello");
  auto Data = encode(ES.get());
  checkASTHeader(Data);
  checkTrivialOrigin(Data);
  check(Data, EncodeKind::ExprString);
  check(Data, "hello"s);
  checkEmpty(Data);
}

TEST_F(SerializeTest, ExprVar) {
  auto EV = std::make_unique<nix::ExprVar>(somePos(), ST.create("hello"));
  EV->level = 43;
  EV->displ = 42;
  EV->fromWith = true;
  auto Data = encode(EV.get());
  checkASTHeader(Data);
  checkTrivialOrigin(Data);
  check(Data, EncodeKind::ExprVar);
  checkSomePos(Data);
  check(Data, "hello"s);
  check(Data, true);
  check(Data, 43);
  check(Data, 42);
  checkEmpty(Data);
}

} // namespace
