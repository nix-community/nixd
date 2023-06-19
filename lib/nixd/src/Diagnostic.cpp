#include "nixd/Diagnostic.h"
#include "nixd/Position.h"
#include "nixd/nix/PosAdapter.h"

#include "lspserver/Logger.h"
#include "lspserver/Protocol.h"

#include <nix/ansicolor.hh>

#include <exception>
#include <optional>

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
namespace nixd {

std::string stripANSI(std::string Msg) {
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

using namespace lspserver;

std::map<std::string, std::vector<lspserver::Diagnostic>>
mkDiagnostics(const nix::BaseError &Err) {
  std::map<std::string, std::vector<lspserver::Diagnostic>> Result;
  auto ErrPos = Err.info().errPos;
  Result[pathOf(ErrPos.get())].emplace_back(
      mkDiagnosic(ErrPos.get(), Err.info().msg.str()));

  if (Err.hasTrace()) {
    for (const auto &T : Err.info().traces) {
      Result[pathOf(T.pos.get())].emplace_back(mkDiagnosic(T, /* Info */ 3));
    }
  }

  return Result;
}

void insertDiagnostic(const nix::BaseError &E,
                      std::vector<lspserver::PublishDiagnosticsParams> &R,
                      std::optional<uint64_t> Version) {
  auto ErrMap = mkDiagnostics(E);
  for (const auto &[Path, DiagVec] : ErrMap) {
    try {
      lspserver::PublishDiagnosticsParams Params;
      Params.uri = lspserver::URIForFile::canonicalize(Path, Path);
      Params.diagnostics = DiagVec;
      Params.version = Version;
      R.emplace_back(std::move(Params));
    } catch (std::exception &E) {
      lspserver::log("{0} while inserting it's diagnostic",
                     stripANSI(E.what()));
    }
  }
}

lspserver::Diagnostic mkDiagnosic(const nix::AbstractPos *Pos, std::string Msg,
                                  int Serverity) {
  lspserver::Diagnostic Diag;
  lspserver::Position LPos;
  if (Pos)
    LPos = toLSPPos(*Pos);
  else
    LPos = {0, 0};
  Diag.message = stripANSI(std::move(Msg));
  Diag.range = {LPos, LPos};
  Diag.severity = Serverity;
  return Diag;
}
} // namespace nixd
