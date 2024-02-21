#include "ASTReader.h"

#include "nixt/Deserialize.h"

#include <nixbc/FileHeader.h>
#include <nixbc/Nodes.h>
#include <nixbc/Origin.h>
#include <nixbc/Type.h>

#include <bc/Read.h>

#include <memory>

namespace nixt {

using namespace nixbc;
using bc::readBytecode;

nix::Expr *ASTDeserializer::eatHookable(std::string_view &Data, ValueMap &VMap,
                                        EnvMap &EMap) {
  auto Kind = eat<ExprKind>(Data);
  switch (Kind) {
  case EK_Int: {
    auto Handle = eat<std::uintptr_t>(Data);
    return Ctx.Pool.record(
        new HookExprInt(eatExprInt(Data), VMap, EMap, Handle));
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

nix::Expr *deserializeHookable(std::string_view &Data, DeserializeContext &Ctx,
                               ValueMap &VMap, EnvMap &EMap) {
  FileHeader Hdr;
  readBytecode(Data, Hdr);
  assert(Hdr.Magic == FileHeader::MagicValue && "Invalid magic value");
  assert(Hdr.Version == 1 && "Invalid version value");

  // Deserialize "Origin"
  Origin::OriginKind Kind;
  readBytecode(Data, Kind);
  nix::Pos::Origin O;
  switch (Kind) {
  case Origin::OK_Path: {
    OriginPath Path;
    readBytecode(Data, Path);
    // TODO
    break;
  }
  case Origin::OK_None:
  case Origin::OK_Stdin:
  case Origin::OK_String:
    break;
  }

  // Deserialize "Hookable"
  ASTDeserializer D(Ctx, O);
  return D.eatHookable(Data, VMap, EMap);
}

} // namespace nixt
