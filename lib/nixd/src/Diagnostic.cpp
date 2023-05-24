#include "nixd/Diagnostic.h"

#include "lspserver/Protocol.h"

#include <nix/ansicolor.hh>

#define FOREACH_ANSI_COLOR(FUNC)                                               \
  FUNC(ANSI_NORMAL)                                                            \
  FUNC(ANSI_BOLD)                                                              \
  FUNC(ANSI_FAINT)                                                             \
  FUNC(ANSI_ITALIC)                                                            \
  FUNC(ANSI_RED)                                                               \
  FUNC(ANSI_GREEN)                                                             \
  FUNC(ANSI_WARNING)                                                           \
  FUNC(ANSI_BLUE)                                                              \
  FUNC(ANSI_MAGENTA)                                                           \
  FUNC(ANSI_CYAN)

static std::string stripANSI(std::string Msg){
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#define REMOVE_ANSI_STR_FUNC(ANSI_STR)                                         \
  do {                                                                         \
    auto Pos = Msg.find(ANSI_STR);                                             \
    if (Pos == std::string::npos)                                              \
      break;                                                                   \
    Msg.erase(Pos, std::strlen(ANSI_STR));                                     \
  } while (true);
  FOREACH_ANSI_COLOR(REMOVE_ANSI_STR_FUNC)
#undef REMOVE_ANSI_STR_FUNC
#pragma GCC diagnostic pop
  return Msg;
}

namespace nixd {

std::vector<lspserver::Diagnostic> mkDiagnostics(const nix::Error &Err) {
  std::vector<lspserver::Diagnostic> Ret;
  auto ErrPos = Err.info().errPos;
  lspserver::Position ErrLoc =
      ErrPos ? translatePosition(*ErrPos) : lspserver::Position{};
  Ret.push_back(
      lspserver::Diagnostic{.range = {.start = ErrLoc, .end = ErrLoc},
                            .severity = /* Error */ 1,
                            .message = stripANSI(Err.info().msg.str())});
  return Ret;
}

lspserver::Position translatePosition(const nix::AbstractPos &P) {
  return {.line = static_cast<int>(P.line - 1),
          .character = static_cast<int>(P.column - 1)};
}

lspserver::Position translatePosition(const nix::Pos &Pos) {
  return {static_cast<int>(Pos.line - 1), static_cast<int>(Pos.column - 1)};
}

} // namespace nixd
