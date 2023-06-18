#pragma once

#include "nixd/Position.h"

#include "lspserver/Protocol.h"

#include <nix/error.hh>
#include <nix/nixexpr.hh>

#include <optional>
#include <utility>

namespace nixd {

std::string stripANSI(std::string Msg);

inline lspserver::Diagnostic mkDiagnosic(const nix::AbstractPos *Pos,
                                         std::string Msg,
                                         int Serverity = /*Error*/ 1) {
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

inline lspserver::Diagnostic mkDiagnosic(const nix::Trace &T,
                                         int Serverity = /*Error*/ 1) {
  return mkDiagnosic(T.pos.get(), T.hint.str(), Serverity);
}

// Maps filepath -> diagnostics
std::map<std::string, std::vector<lspserver::Diagnostic>>
mkDiagnostics(const nix::BaseError &);

void insertDiagnostic(const nix::BaseError &E,
                      std::vector<lspserver::PublishDiagnosticsParams> &R,
                      std::optional<uint64_t> Version = std::nullopt);

} // namespace nixd
