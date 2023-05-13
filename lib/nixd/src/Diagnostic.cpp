#include "nixd/Diagnostic.h"

#include "lspserver/Protocol.h"

namespace nixd {

std::vector<lspserver::Diagnostic> mkDiagnostics(const nix::Error &PE) {
  std::vector<lspserver::Diagnostic> Ret;
  auto ErrPos = PE.info().errPos;
  lspserver::Position ErrLoc =
      ErrPos ? translatePosition(*ErrPos) : lspserver::Position{};
  Ret.push_back(lspserver::Diagnostic{.range = {.start = ErrLoc, .end = ErrLoc},
                                      .severity = /* Error */ 1,
                                      .message = PE.info().msg.str()});
  return Ret;
}

lspserver::Position translatePosition(const nix::AbstractPos &P) {
  return {.line = static_cast<int>(P.line - 1),
          .character = static_cast<int>(P.column - 1)};
}

} // namespace nixd
