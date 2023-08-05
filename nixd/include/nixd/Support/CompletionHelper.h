#pragma once

#include "lspserver/Protocol.h"

#include <nix/eval.hh>

#include <vector>

namespace nixd {

struct CompletionHelper {
  using Items = std::vector<lspserver::CompletionItem>;
  static void fromEnv(nix::EvalState &State, nix::Env &NixEnv, Items &Items);
  static void fromStaticEnv(const nix::SymbolTable &STable,
                            const nix::StaticEnv &SEnv, Items &Items);
};

} // namespace nixd
