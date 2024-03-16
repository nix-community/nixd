/// \file
/// \brief Implementation of [Hover Request].
/// [Hover Request]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_hover

#include "Controller.h"
#include "Convert.h"

#include <llvm/Support/Error.h>

#include <boost/asio/post.hpp>

#include <mutex>

namespace nixd {

using namespace llvm::json;
using namespace lspserver;
using namespace rpc;

void Controller::onHover(const TextDocumentPositionParams &Params,
                         Callback<std::optional<Hover>> Reply) {
  auto Action = [Reply = std::move(Reply),
                 File = std::string(Params.textDocument.uri.file()),
                 RawPos = Params.position, this]() mutable {
    std::lock_guard G(TUsLock);
    const std::shared_ptr<nixf::Node> &AST = TUs[File].ast();
    nixf::Position Pos{RawPos.line, RawPos.character};
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
  };
  boost::asio::post(Pool, std::move(Action));
}

} // namespace nixd
