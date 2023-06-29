#include "nixd/Server.h"

#include <nix/eval-inline.hh>
#include <nix/shared.hh>

namespace nix {

// Copy-paste from nix source code, do not know why it is inlined.

inline void EvalState::evalAttrs(Env &env, Expr *e, Value &v, const PosIdx pos,
                                 std::string_view errorCtx) {
  try {
    e->eval(*this, env, v);
    if (v.type() != nAttrs)
      error("value is %1% while a set was expected", showType(v))
          .withFrame(env, *e)
          .debugThrow<TypeError>();
  } catch (Error &e) {
    e.addTrace(positions[pos], errorCtx);
    throw;
  }
}

} // namespace nix
namespace nixd {

void CompletionHelper::fromStaticEnv(const nix::SymbolTable &STable,
                                     const nix::StaticEnv &SEnv, Items &Items) {
  for (auto [Symbol, Displ] : SEnv.vars) {
    std::string Name = STable[Symbol];
    if (Name.starts_with("__"))
      continue;

    // Variables in static envs, let's mark it as "Constant".
    Items.emplace_back(lspserver::CompletionItem{
        .label = Name, .kind = lspserver::CompletionItemKind::Constant});
  }
  if (SEnv.up)
    fromStaticEnv(STable, SEnv, Items);
}

void CompletionHelper::fromEnv(nix::EvalState &State, nix::Env &NixEnv,
                               Items &Items) {
  if (NixEnv.type == nix::Env::HasWithExpr) {
    Value *V = State.allocValue();
    State.evalAttrs(*NixEnv.up, (nix::Expr *)NixEnv.values[0], *V, noPos,
                    "<borked>");
    NixEnv.values[0] = V;
    NixEnv.type = Env::HasWithAttrs;
  }
  if (NixEnv.type == nix::Env::HasWithAttrs) {
    for (const auto &SomeAttr : *NixEnv.values[0]->attrs) {
      std::string Name = State.symbols[SomeAttr.name];
      if (Items.size() > 5000)
        break;
      Items.emplace_back(lspserver::CompletionItem{
          .label = Name, .kind = lspserver::CompletionItemKind::Variable});
    }
  }
  if (NixEnv.up)
    fromEnv(State, *NixEnv.up, Items);
}

void Server::initWorker() {
  assert(Role == ServerRole::Controller &&
         "Must switch from controller's fork!");
  nix::initNix();
  nix::initLibStore();
  nix::initPlugins();
  nix::initGC();
}

} // namespace nixd
