#pragma once

#include "nixd/Support/Position.h"

#include "lspserver/Protocol.h"

#include <nix/error.hh>
#include <nix/nixexpr.hh>

namespace nixd {

std::string stripANSI(std::string Msg);

lspserver::Diagnostic mkDiagnosic(const nix::AbstractPos *Pos, std::string Msg,
                                  int Serverity = /*Error*/ 1);

inline lspserver::Diagnostic mkDiagnosic(const nix::Trace &T,
                                         int Serverity = /*Error*/ 1) {
  return mkDiagnosic(T.pos.get(), T.hint.str(), Serverity);
}

// Maps filepath -> diagnostics
std::map<std::string, std::vector<lspserver::Diagnostic>>
mkDiagnostics(const nix::BaseError &);

void insertDiagnostic(
    const nix::BaseError &E,
    std::map<std::string, lspserver::PublishDiagnosticsParams> &R,
    std::optional<uint64_t> Version = std::nullopt);

inline void
insertDiagnostic(const nix::ErrorInfo &E,
                 std::map<std::string, lspserver::PublishDiagnosticsParams> &R,
                 std::optional<uint64_t> Version = std::nullopt) {
  return insertDiagnostic(nix::BaseError(E), R, Version);
}
} // namespace nixd
