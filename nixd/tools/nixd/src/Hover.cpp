/// \file
/// \brief Implementation of [Hover Request].
/// [Hover Request]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_hover

#include "Controller.h"
#include "Convert.h"

namespace nixd {

using namespace llvm::json;
using namespace lspserver;

void Controller::onHover(const TextDocumentPositionParams &Params,
                         Callback<std::optional<Hover>> Reply) {
  PathRef File = Params.textDocument.uri.file();
  const nixf::Node *AST = TUs[File].ast().get();
  nixf::Position Pos{Params.position.line, Params.position.character};
  const nixf::Node *N = AST->descend({Pos, Pos});
  if (!N) {
    Reply(std::nullopt);
    return;
  }
  std::string Name = N->name();
  Reply(Hover{
      .contents =
          MarkupContent{
              .kind = MarkupKind::Markdown,
              .value = "`" + Name + "`",
          },
      .range = toLSPRange(N->range()),
  });
}

} // namespace nixd
