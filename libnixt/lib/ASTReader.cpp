#include "ASTReader.h"

#include "nixt/Deserialize.h"

#include <nixbc/FileHeader.h>
#include <nixbc/Nodes.h>
#include <nixbc/Origin.h>
#include <nixbc/Type.h>

#include <bc/Read.h>

#include <nix/eval.hh>
#include <nix/fs-input-accessor.hh>

namespace nixt {

using namespace nixbc;
nix::Expr *ASTDeserializer::eatHookable(std::string_view &Data, ValueMap &VMap,
                                        EnvMap &EMap) {
  auto Hdr = eat<NodeHeader>(Data);
  switch (Hdr.Kind) {
  case EK_Int: {
    return Pool.record(
        new HookExprInt(eatExprInt(Data), VMap, EMap, Hdr.Handle));
  }
  default:
    break;
  }
  assert(false && "Unknown hookable kind");
  __builtin_unreachable();
  return nullptr;
}

nix::Symbol ASTDeserializer::eatSymbol(std::string_view &Data) {
  return Ctx.STable.create(eat<std::string>(Data));
}

nix::ExprInt ASTDeserializer::eatExprInt(std::string_view &Data) {
  return {eat<NixInt>(Data)};
}

DeserializeContext getDeserializeContext(nix::EvalState &State,
                                         std::string_view BasePath,
                                         const nix::Pos::Origin &Origin) {
  auto Base = State.rootPath(nix::CanonPath(BasePath));
  const nix::ref<nix::InputAccessor> RootFS = State.rootFS;

  // Filling the context.
  // These are required for constructing evaluable nix ASTs
  return {.STable = State.symbols,
          .PTable = State.positions,
          .BasePath = Base,
          .RootFS = RootFS,
          .Origin = Origin};
}

nix::Expr *deserializeHookable(std::string_view &Data, DeserializeContext &Ctx,
                               PtrPool<nix::Expr> &Pool, ValueMap &VMap,
                               EnvMap &EMap) {
  // Deserialize "Hookable"
  ASTDeserializer D(Ctx, Pool);
  return D.eatHookable(Data, VMap, EMap);
}

} // namespace nixt
