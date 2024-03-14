/// \file
/// \brief Implementation of [Code Action].
/// [Code Action]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_codeAction

#include "Controller.h"
#include "Convert.h"

namespace nixd {

using namespace llvm::json;
using namespace lspserver;

void Controller::onCodeAction(const lspserver::CodeActionParams &Params,
                              Callback<std::vector<CodeAction>> Reply) {
  PathRef File = Params.textDocument.uri.file();
  Range Range = Params.range;
  const std::vector<nixf::Diagnostic> &Diagnostics = TUs[File].diagnostics();
  std::vector<CodeAction> Actions;
  Actions.reserve(Diagnostics.size());
  for (const nixf::Diagnostic &D : Diagnostics) {
    auto DRange = toLSPRange(D.range());
    if (!Range.overlap(DRange))
      continue;

    // Add fixes.
    for (const nixf::Fix &F : D.fixes()) {
      std::vector<TextEdit> Edits;
      Edits.reserve(F.edits().size());
      for (const nixf::TextEdit &TE : F.edits()) {
        Edits.emplace_back(TextEdit{
            .range = toLSPRange(TE.oldRange()),
            .newText = std::string(TE.newText()),
        });
      }
      using Changes = std::map<std::string, std::vector<TextEdit>>;
      std::string FileURI = URIForFile::canonicalize(File, File).uri();
      WorkspaceEdit WE{.changes = Changes{
                           {std::move(FileURI), std::move(Edits)},
                       }};
      Actions.emplace_back(CodeAction{
          .title = F.message(),
          .kind = std::string(CodeAction::QUICKFIX_KIND),
          .edit = std::move(WE),
      });
    }
  }
  Reply(std::move(Actions));
}

} // namespace nixd
