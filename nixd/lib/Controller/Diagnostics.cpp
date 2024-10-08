/// \file
/// \brief Implementation of [PublishDiagnostics Notification].
/// [PublishDiagnostics Notification]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_publishDiagnostics

#include "Convert.h"

#include "nixd/Controller/Controller.h"

#include <nixf/Basic/Diagnostic.h>

#include <mutex>
#include <optional>

using namespace nixd;
using namespace llvm::json;
using namespace lspserver;

void Controller::updateSuppressed(const std::vector<std::string> &Sup) {
  // Acquire the lock because the set needs to be written.
  std::lock_guard _(SuppressedDiagnosticsLock);
  // Clear the set, just construct a new one.
  SuppressedDiagnostics.clear();

  // For each element, see if the name matches some declared name.
  // If so, insert the set.
  for (const auto &Name : Sup) {
    if (auto DK = nixf::Diagnostic::parseKind(Name)) {
      SuppressedDiagnostics.insert(*DK);
    } else {
      // The name is not listed in knwon names. Log error here
      lspserver::elog("diagnostic suppressing sname {0} is unknown", Name);
    }
  }
}

bool Controller::isSuppressed(nixf::Diagnostic::DiagnosticKind Kind) {
  std::lock_guard _(SuppressedDiagnosticsLock);
  return SuppressedDiagnostics.contains(Kind);
}

void Controller::publishDiagnostics(
    PathRef File, std::optional<int64_t> Version, std::string_view Src,
    const std::vector<nixf::Diagnostic> &Diagnostics) {
  std::vector<Diagnostic> LSPDiags;
  LSPDiags.reserve(Diagnostics.size());
  for (const nixf::Diagnostic &D : Diagnostics) {
    // Before actually doing anything,
    // let's check if the diagnostic is suppressed.
    // If suppressed, just skip it.
    if (isSuppressed(D.kind()))
      continue;

    // Format the message.
    std::string Message = D.format();

    // Add fix information.
    if (!D.fixes().empty()) {
      Message += " (";
      if (D.fixes().size() == 1) {
        Message += "fix available";
      } else {
        Message += std::to_string(D.fixes().size());
        Message += " fixes available";
      }
      Message += ")";
    }

    Diagnostic &Diag = LSPDiags.emplace_back(Diagnostic{
        .range = toLSPRange(Src, D.range()),
        .severity = getLSPSeverity(D.kind()),
        .code = D.sname(),
        .source = "nixf",
        .message = Message,
        .tags = toLSPTags(D.tags()),
        .relatedInformation = std::vector<DiagnosticRelatedInformation>{},
    });

    assert(Diag.relatedInformation && "Must be initialized");
    Diag.relatedInformation->reserve(D.notes().size());
    for (const nixf::Note &N : D.notes()) {
      Diag.relatedInformation->emplace_back(DiagnosticRelatedInformation{
          .location =
              Location{
                  .uri = URIForFile::canonicalize(File, File),
                  .range = toLSPRange(Src, N.range()),
              },
          .message = N.format(),
      });
    }
    auto Notes = D.notes();
    auto DRange = Diag.range;

    for (const nixf::Note &N : Notes) {
      LSPDiags.emplace_back(Diagnostic{
          .range = toLSPRange(Src, N.range()),
          .severity = 4,
          .code = N.sname(),
          .source = "nixf",
          .message = N.format(),
          .tags = toLSPTags(N.tags()),
          .relatedInformation =
              std::vector<DiagnosticRelatedInformation>{
                  DiagnosticRelatedInformation{
                      .location =
                          Location{
                              .uri = URIForFile::canonicalize(File, File),
                              .range = DRange,
                          },
                      .message = "original diagnostic",
                  }},
      });
    }
  }
  PublishDiagnostic({
      .uri = URIForFile::canonicalize(File, File),
      .diagnostics = std::move(LSPDiags),
      .version = Version,
  });
}
