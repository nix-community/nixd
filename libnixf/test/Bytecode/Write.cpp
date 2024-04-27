#include <gtest/gtest.h>

#include "bc/Read.h"

#include "nixbc/Nodes.h"

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Bytecode/Write.h"
#include "nixf/Parse/Parser.h"

#include <sstream>
#include <string_view>

namespace {

struct WriteTest : testing::Test {
  std::vector<nixf::Diagnostic> Diags;
  std::ostringstream OS;
};

TEST_F(WriteTest, Int) {
  auto AST = nixf::parse("1", Diags);
  ASSERT_TRUE(AST);
  nixf::writeBytecode(OS, AST.get());
  std::string Str = OS.str();
  std::string_view Data(Str);

  ASSERT_EQ(Str.size(), 32);

  auto Kind = bc::eat<nixbc::ExprKind>(Data);

  ASSERT_EQ(Kind, nixbc::ExprKind::EK_Int);
}

TEST_F(WriteTest, Float) {
  auto AST = nixf::parse("1.2", Diags);
  ASSERT_TRUE(AST);
  nixf::writeBytecode(OS, AST.get());
  std::string Str = OS.str();
  std::string_view Data(Str);

  ASSERT_EQ(Str.size(), 32);

  auto Kind = bc::eat<nixbc::ExprKind>(Data);

  ASSERT_EQ(Kind, nixbc::ExprKind::EK_Float);
}

TEST_F(WriteTest, String) {
  auto AST = nixf::parse(R"("a")", Diags);
  ASSERT_TRUE(AST);
  nixf::writeBytecode(OS, AST.get());
  std::string Str = OS.str();
  std::string_view Data(Str);

  ASSERT_EQ(Str.size(), 33);

  auto Kind = bc::eat<nixbc::ExprKind>(Data);

  ASSERT_EQ(Kind, nixbc::ExprKind::EK_String);
}

TEST_F(WriteTest, Path) {
  auto AST = nixf::parse("./a", Diags);
  ASSERT_TRUE(AST);
  nixf::writeBytecode(OS, AST.get());
  std::string Str = OS.str();
  std::string_view Data(Str);

  ASSERT_EQ(Str.size(), 35);

  auto Kind = bc::eat<nixbc::ExprKind>(Data);

  ASSERT_EQ(Kind, nixbc::ExprKind::EK_Path);
}

TEST_F(WriteTest, Var) {
  auto AST = nixf::parse(R"(a)", Diags);
  ASSERT_TRUE(AST);
  nixf::writeBytecode(OS, AST.get());
  std::string Str = OS.str();
  std::string_view Data(Str);

  ASSERT_EQ(Str.size(), 33);

  auto Kind = bc::eat<nixbc::ExprKind>(Data);

  ASSERT_EQ(Kind, nixbc::ExprKind::EK_Var);
}

TEST_F(WriteTest, Select) {
  auto AST = nixf::parse(R"(a.b.c)", Diags);
  ASSERT_TRUE(AST);
  nixf::writeBytecode(OS, AST.get());
  std::string Str = OS.str();
  std::string_view Data(Str);

  ASSERT_EQ(Str.size(), 155);

  auto Kind = bc::eat<nixbc::ExprKind>(Data);

  ASSERT_EQ(Kind, nixbc::ExprKind::EK_Select);
}

TEST_F(WriteTest, SelectNullAttr) {
  auto AST = nixf::parse(R"(a.b.)", Diags);
  ASSERT_TRUE(AST);
  nixf::writeBytecode(OS, AST.get());
  std::string Str = OS.str();
  std::string_view Data(Str);

  ASSERT_EQ(Str.size(), 122);

  auto Kind = bc::eat<nixbc::ExprKind>(Data);

  ASSERT_EQ(Kind, nixbc::ExprKind::EK_Select);
}

TEST_F(WriteTest, SPath) {
  auto AST = nixf::parse(R"(<nixpkgs>)", Diags);
  ASSERT_TRUE(AST);
  nixf::writeBytecode(OS, AST.get());
  std::string Str = OS.str();
  std::string_view Data(Str);

  ASSERT_EQ(Str.size(), 154);

  auto Kind = bc::eat<nixbc::ExprKind>(Data);

  ASSERT_EQ(Kind, nixbc::ExprKind::EK_Call);
}

const char *AllGrammar = R"(
let
  x = 1;
in
rec {
  path = ./.;
  hasAttr = someAttrSet ? "a";
  someAttrSet = { a = 1; };
  integerAnd = 1 && 0;
  floatAdd = 1.5 + 2.0;
  ops = (1 >= 2) - (1 <= 2) * 3 + 1 / 2;
  ifElse = if 1 then 1 else 0;
  update = { } // { };
  opEq = 1 == 2;
  opNEq = 1 != 2;
  opImpl = 1 -> 2;
  opNot = ! false;
  opOr = 1 || 2;
  lambda = {}: { };
  string = "someString";
  listConcat = [ 1 2 ] ++ [ 3 4 ];
  concat = "a" + "b";
  exprDiv = 5 / 3;
  exprPos = __curPos;
  exprAssert = assert true ; "some message";
  select = someAttrSet.a;
  exprWith = with someAttrSet; [ ];
}
)";

TEST_F(WriteTest, Complex) {
  auto AST = nixf::parse(AllGrammar, Diags);
  ASSERT_TRUE(AST);
  nixf::writeBytecode(OS, AST.get());
  std::string Str = OS.str();
  std::string_view Data(Str);

  ASSERT_EQ(Str.size(), 3334);

  auto Kind = bc::eat<nixbc::ExprKind>(Data);

  ASSERT_EQ(Kind, nixbc::ExprKind::EK_Let);
}

} // namespace
