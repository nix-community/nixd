#include "nixt/Deserialize.h"
#include "nixt/HookExpr.h"
#include "nixt/InitEval.h"
#include "nixt/PtrPool.h"

namespace {

// --eval, -e
// Should we perform eval?
bool Eval = false;

void parseArgs(int Argc, const char *Argv[]) {
  for (int I = 0; I < Argc; I++) {
    std::string_view Arg(Argv[I]);
    if (Arg == "--eval" || Arg == "-e")
      Eval = true;
  }
}

} // namespace

int main(int Argc, const char *Argv[]) {
  parseArgs(Argc, Argv);
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

  if (Eval) {
    try {
      AST->bindVars(*State, State->staticBaseEnv);
      nix::Value V;
      State->eval(AST, V);
      V.print(State->symbols, std::cout);
    } catch (nix::BaseError &E) {
      std::cerr << E.what() << "\n";
    }
  } else {
    AST->show(State->symbols, std::cout);
  }

  return 0;
}
