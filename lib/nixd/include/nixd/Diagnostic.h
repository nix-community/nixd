#pragma once
#include "lspserver/Protocol.h"

#include <nixexpr.hh>

namespace nixd {

std::vector<lspserver::Diagnostic> mkDiagnostics(const nix::BaseError &);

lspserver::Position translatePosition(const nix::AbstractPos &P);

lspserver::Position translatePosition(const nix::Pos &Pos);

} // namespace nixd
