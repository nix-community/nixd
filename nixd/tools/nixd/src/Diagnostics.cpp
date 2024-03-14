/// \file
/// \brief Implementation of [PublishDiagnostics Notification].
/// [PublishDiagnostics Notification]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_publishDiagnostics

#include "Controller.h"
#include "Convert.h"

namespace nixd {

using namespace llvm::json;
using namespace lspserver;

void Controller::publishDiagnostics(
    PathRef File, std::optional<int64_t> Version,
    const std::vector<nixf::Diagnostic> &Diagnostics) {
  std::vector<Diagnostic> LSPDiags;
  LSPDiags.reserve(Diagnostics.size());
  for (const nixf::Diagnostic &D : Diagnostics) {
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
        .range = toLSPRange(D.range()),
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
                  .range = toLSPRange(N.range()),
              },
          .message = N.format(),
      });
      LSPDiags.emplace_back(Diagnostic{
          .range = toLSPRange(N.range()),
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
                              .range = Diag.range,
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

} // namespace nixd
