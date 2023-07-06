#include "nixd/Server/ASTManager.h"
#include "nixd/Parser/Require.h"

#include <boost/asio/post.hpp>

#include <optional>

namespace nixd {

void ASTManager::withAST(const std::string &Path, VersionTy Version,
                         ActionTy Action) {
  {
    std::lock_guard _(ActionsLock);
    Actions.insert({Path, std::move(Action)});
  }
  checkCacheAndInvoke(Path, Version);
}

void ASTManager::invokeActions(const ParseAST &AST, const std::string &Path,
                               VersionTy Version) {
  // The actions that will be invoked are stored in this vector
  std::vector<ActionTy> Acts;

  {
    std::lock_guard _(ActionsLock);
    auto [EqBegin, EqEnd] = Actions.equal_range(Path);
    for (auto I = EqBegin; I != EqEnd; I++) {
      auto &[_, Act] = *I;
      Acts.emplace_back(std::move(Act));
    }
    Actions.erase(EqBegin, EqEnd);
  }

  for (auto &Act : Acts) {
    Act(AST, Version);
  }
}

bool ASTManager::checkCacheAndInvoke(const std::string &Path,
                                     VersionTy Version) {
  auto CheckCache = [this](const std::string &Path, VersionTy Version)
      -> std::optional<std::pair<ParseAST *, ASTManager::VersionTy>> {
    if (ASTCache.contains(Path)) {
      auto &[CachedAST, CachedVersion] = ASTCache.at(Path);
      if (CachedVersion >= Version)
        return std::pair{CachedAST.get(), CachedVersion};
    }
    return std::nullopt;
  };
  std::lock_guard _(ASTCacheLock);
  if (const auto Cache = CheckCache(Path, Version)) {
    auto [AST, CachedVersion] = *Cache;
    // The AST pointer must be guarded by the cache lock
    invokeActions(*AST, Path, CachedVersion);
    return true;
  }
  return false;
}

void ASTManager::schedParse(const std::string &Content, const std::string &Path,
                            VersionTy Version) {
  auto Task = [=, this]() {
    try {
      if (checkCacheAndInvoke(Path, Version))
        return;
      auto ParseData = parse(Content, Path);
      auto NewAST = std::make_unique<ParseAST>((std::move(ParseData)));

      // TODO: use AST builder to unify these stuff
      NewAST->bindVars();
      NewAST->staticAnalysis();

      invokeActions(*NewAST, Path, Version);

      // Update the cache
      {
        std::lock_guard _(ASTCacheLock);
        ASTCache[Path] = std::make_pair(std::move(NewAST), Version);
      }
    } catch (...) {
      // TODO: do something here?
    }
  };

  boost::asio::post(Pool, std::move(Task));
}

} // namespace nixd
