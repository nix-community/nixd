#include "nixt/Deserialize.h"
#include "nixt/HookExpr.h"
#include "nixt/InitEval.h"
#include "nixt/PtrPool.h"

int main() {
  nixt::initEval();
  std::unique_ptr<nix::EvalState> State(
      new nix::EvalState{{}, nix::openStore("dummy://")});

  std::ostringstream Inputs;
  Inputs << std::cin.rdbuf();
  std::string DataBuf = Inputs.str();

  std::string_view Data(DataBuf);

  nixt::PtrPool<nix::Expr> Pool;

  nixt::ValueMap VMap;
  nixt::EnvMap EMap;
  nix::Pos::Origin O = nix::Pos::none_tag{};

  nixt::DeserializeContext Ctx = nixt::getDeserializeContext(*State, ".", O);

  auto *AST = nixt::deserializeHookable(Data, Ctx, Pool, VMap, EMap);

  AST->show(State->symbols, std::cout);

  return 0;
}
