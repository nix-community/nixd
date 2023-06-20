#include "nixd/Server.h"

#include <nix/shared.hh>

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
}

void CompletionHelper::fromEnvWith(const nix::SymbolTable &STable,
                                   const nix::Env &NixEnv, Items &Items) {
  if (NixEnv.type == nix::Env::HasWithAttrs) {
    for (const auto &SomeAttr : *NixEnv.values[0]->attrs) {
      std::string Name = STable[SomeAttr.name];
      Items.emplace_back(lspserver::CompletionItem{
          .label = Name, .kind = lspserver::CompletionItemKind::Variable});
    }
  }
}

void CompletionHelper::fromEnvRecursive(const nix::SymbolTable &STable,
                                        const nix::StaticEnv &SEnv,
                                        const nix::Env &NixEnv, Items &Items) {

  if ((SEnv.up != nullptr) && (NixEnv.up != nullptr))
    fromEnvRecursive(STable, *SEnv.up, *NixEnv.up, Items);

  fromStaticEnv(STable, SEnv, Items);
  fromEnvWith(STable, NixEnv, Items);
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
