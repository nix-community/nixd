#pragma once

#include "nixd/AST/ParseAST.h"

#include "lspserver/Protocol.h"

#include <map>

namespace nixd {

class AttrLocator {
  std::map<lspserver::Range, const nix::Expr *> Attrs;

public:
  AttrLocator(const ParseAST &AST);

  /// Locate a nix attribute, \return the pointer to the expression
  /// { a = "1"; }
  ///   ^   ^~~
  ///  Pos  pointer
  [[nodiscard]] const nix::Expr *locate(lspserver::Position Pos) const;
};

} // namespace nixd
