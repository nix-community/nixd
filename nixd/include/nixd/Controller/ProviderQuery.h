#pragma once

#include "ProviderRegistry.h"

#include <llvm/Support/Error.h>

#include <utility>

namespace nixd {

template <typename Result, typename Query>
Result queryProvider(ProviderRegistry &Registry,
                     ProviderRegistry::QueryToken Token, Result Fallback,
                     Query &&Action) {
  auto Worker = Token.worker();
  auto Response = std::forward<Query>(Action)(*Worker);
  if (!Response) {
    llvm::consumeError(Response.takeError());
    if (!Worker->alive())
      Registry.queryFailed(Token);
    return Fallback;
  }

  if (!Worker->alive()) {
    Registry.queryFailed(Token);
    return Fallback;
  }
  if (!Registry.validate(Token))
    return Fallback;
  return std::move(*Response);
}

template <typename Result, typename Query>
Result queryProvider(ProviderRegistry &Registry, const ProviderKey &Key,
                     Result Fallback, Query &&Action) {
  auto Token = Registry.acquire(Key);
  if (!Token)
    return Fallback;
  return queryProvider(Registry, std::move(*Token), std::move(Fallback),
                       std::forward<Query>(Action));
}

} // namespace nixd
