#pragma once

#include "Diagnostic.h"
#include "Range.h"

namespace nixf {

class DiagnosticEngine {
public:
  Diagnostic &diag(Diagnostic::DiagnosticKind Kind, OffsetRange Range) {
    auto Diag = std::make_unique<Diagnostic>(Kind, Range);
    Diags.emplace_back(std::move(Diag));
    return *Diags.back();
  }

  std::vector<std::unique_ptr<Diagnostic>> &diags() { return Diags; }

  [[nodiscard]] Diagnostic::Severity
  getServerity(Diagnostic::DiagnosticKind Kind) const {
    return Diagnostic::severity(Kind);
  }

private:
  std::vector<std::unique_ptr<Diagnostic>> Diags;
};

} // namespace nixf
