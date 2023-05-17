#pragma once
#include "lspserver/Protocol.h"

#include <nixexpr.hh>

namespace nixd {

std::vector<lspserver::Diagnostic> mkDiagnostics(const nix::Error &);

lspserver::Position translatePosition(const nix::AbstractPos &P);

} // namespace nixd
