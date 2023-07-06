#pragma once

#include "nixd/AST/ParseAST.h"

#include <llvm/ADT/FunctionExtras.h>

#include <boost/asio/thread_pool.hpp>

#include <mutex>
#include <queue>
#include <string>

namespace nixd {

class ASTManager {
public:
  using VersionTy = int64_t;
  using ActionTy =
      llvm::unique_function<void(const ParseAST &AST, VersionTy &Version)>;
  using CachedASTTy = std::pair<std::unique_ptr<ParseAST>, VersionTy>;

private:
  boost::asio::thread_pool &Pool;

  std::multimap<std::string, ActionTy> Actions; // GUARDED_BY(ActionsLock)
  std::mutex ActionsLock;

  /// Path -> {AST, Version}
  std::map<std::string, CachedASTTy> ASTCache; // GUARDED_BY(ASTCacheLock)
  std::mutex ASTCacheLock;

  void invokeActions(const ParseAST &AST, const std::string &Path,
                     VersionTy Version);

  bool checkCacheAndInvoke(const std::string &Path, VersionTy Version);

public:
  ASTManager(boost::asio::thread_pool &Pool) : Pool(Pool) {}

  /// Store the action in a local structure, the action will be invoked when the
  /// task finished.
  void withAST(const std::string &Path, ActionTy Action);

  void schedParse(const std::string &Content, const std::string &Path,
                  VersionTy Version);
};

} // namespace nixd
