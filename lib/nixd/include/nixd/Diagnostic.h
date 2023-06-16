#pragma once
#include "lspserver/Protocol.h"

#include <nix/nixexpr.hh>

namespace nixd {

std::string stripANSI(std::string Msg);

std::vector<lspserver::Diagnostic> mkDiagnostics(const nix::BaseError &);

} // namespace nixd
