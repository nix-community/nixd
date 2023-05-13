#include "nixd/Diagnostic.h"

#include "lspserver/Protocol.h"

std::vector<lspserver::Diagnostic> mkDiagnostics(nix::Error PE) {
  std::vector<lspserver::Diagnostic> Ret;
  auto ErrPos = PE.info().errPos;
  lspserver::Position ErrLoc{.line = static_cast<int>(ErrPos->line) - 1,
                             .character = static_cast<int>(ErrPos->column) - 1};
  Ret.push_back(lspserver::Diagnostic{.range = {.start = ErrLoc, .end = ErrLoc},
                                      .severity = /* Error */ 1,
                                      .message = PE.info().msg.str()});
  return Ret;
}
