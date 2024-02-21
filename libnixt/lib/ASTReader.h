#pragma once

#include "nixt/Deserialize.h"
#include "nixt/HookExpr.h"
#include "nixt/PtrPool.h"

#include <bc/Read.h>

#include <nix/nixexpr.hh>

namespace nixt {

class ASTDeserializer {
  DeserializeContext &Ctx;
  [[maybe_unused]] nix::Pos::Origin &Origin;

  template <class T> T eat(std::string_view &Data) { return bc::eat<T>(Data); }

public:
  ASTDeserializer(DeserializeContext &Ctx, nix::Pos::Origin &Origin)
      : Ctx(Ctx), Origin(Origin) {}

  nix::Expr *eatHookable(std::string_view &Data, ValueMap &VMap, EnvMap &EMap);

  nix::Symbol eatSymbol(std::string_view &Data);

  nix::ExprInt eatExprInt(std::string_view &Data);
};

} // namespace nixt
