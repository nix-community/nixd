#pragma once
#include "lspserver/Protocol.h"
#include "nixexpr.hh"

std::vector<lspserver::Diagnostic> mkDiagnostics(nix::ParseError PE);
