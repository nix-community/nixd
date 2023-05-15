#include "nixd/Diagnostic.h"

#include "lspserver/Protocol.h"

using std::string;
#define __REMOVED_ANSI_STR__(_) \
_("\e[0m") \
_("\e[1m") \
_("\e[2m") \
_("\e[3m") \
_("\e[31;1m") \
_("\e[32;1m") \
_("\e[35;1m") \
_("\e[34;1m") \
_("\e[35;1m") \
_("\e[36;1m") \

static std::string ansiFilter(std::string Msg){
    #define __REMOVE_ANSI_STR_CODE__(ansi_str) \
    do { \
        auto Pos = Msg.find(ansi_str); \
        if (Pos==string::npos) break; \
        Msg.erase(Pos, std::strlen(ansi_str)); \
    } while(1);
    __REMOVED_ANSI_STR__(__REMOVE_ANSI_STR_CODE__)
    return Msg;
}

namespace nixd {

std::vector<lspserver::Diagnostic> mkDiagnostics(const nix::Error &PE) {
  std::vector<lspserver::Diagnostic> Ret;
  auto ErrPos = PE.info().errPos;
  lspserver::Position ErrLoc =
      ErrPos ? translatePosition(*ErrPos) : lspserver::Position{};
  Ret.push_back(lspserver::Diagnostic{.range = {.start = ErrLoc, .end = ErrLoc},
                                      .severity = /* Error */ 1,
                                      .message = ansiFilter(PE.info().msg.str())});
  return Ret;
}

lspserver::Position translatePosition(const nix::AbstractPos &P) {
  return {.line = static_cast<int>(P.line - 1),
          .character = static_cast<int>(P.column - 1)};
}

} // namespace nixd
