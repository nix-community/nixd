#include "nixd/Support/CompletionHelper.h"
#include "nixd/Nix/Eval.h"

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
    nix::Value *V = State.allocValue();
    State.evalAttrs(*NixEnv.up, (nix::Expr *)NixEnv.values[0], *V, nix::noPos,
                    "<borked>");
    NixEnv.values[0] = V;
    NixEnv.type = nix::Env::HasWithAttrs;
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

} // namespace nixd
