#include "ReaderData/ReaderData.h"
#include "StateTest.h"

#include "nixt/Deserialize.h"
#include "nixt/InitEval.h"

#include <nix/eval.hh>
#include <nix/store-api.hh>

using namespace nixt;

namespace {

struct ReaderTest : StateTest {
  nix::Pos::Origin O = nix::Pos::none_tag{};
  DeserializeContext Ctx;
  ValueMap VMap;
  EnvMap EMap;
  PtrPool<nix::Expr> Pool;
  ReaderTest() : Ctx(getDeserializeContext(*State, ".", O)) {}
};

TEST_F(ReaderTest, AllGrammars) {
  std::string_view Data(reinterpret_cast<const char *>(AllGrammars),
                        AllGrammarsLen);
  nix::Expr *AST = deserializeHookable(Data, Ctx, Pool, VMap, EMap);

  ASSERT_TRUE(dynamic_cast<nix::ExprLet *>(AST));
}

} // namespace
