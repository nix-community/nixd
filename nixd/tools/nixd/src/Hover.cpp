/// \file
/// \brief Implementation of [Hover Request].
/// [Hover Request]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_hover

#include "Controller.h"
#include "Convert.h"

#include "nixd/rpc/Protocol.h"

#include <llvm/Support/Error.h>

#include <boost/asio/post.hpp>

#include <cstdint>
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
    std::string Name = "`" + std::string(N->name()) + "`";
    Hover NoEvalResponse{
        .contents =
            MarkupContent{
                .kind = MarkupKind::Markdown,
                .value = Name,
            },
        .range = toLSPRange(N->range()),
    };
    if (!Eval || !Eval->ready()) {
      Reply(std::move(NoEvalResponse));
      return;
    }

    auto OnResponse = [NoEvalResponse = std::move(NoEvalResponse),
                       Reply = std::move(Reply)](
                          llvm::Expected<ExprValueResponse> Response) mutable {
      if (!Response) {
        Reply(std::move(NoEvalResponse));
        return;
      }
      if (Response->ResultKind == ExprValueResponse::OK) {
        // Response for evaluated nodes.
        Hover EvalResponse(std::move(NoEvalResponse));

        if (Response->ErrorMsg) {
          elog("eval error encountered: {0}", Response->ErrorMsg);
        }
        EvalResponse.contents.value += "\r\n\r\n";
        EvalResponse.contents.value += "Value: ";
        EvalResponse.contents.value +=
            llvm::formatv("{0}", (*Response).Description);
        Reply(std::move(EvalResponse));
      } else {
        Reply(std::move(NoEvalResponse));
      }
    };

    Eval->ExprValue(ExprValueParams{reinterpret_cast<std::int64_t>(N)},
                    std::move(OnResponse));
  };
  boost::asio::post(Pool, std::move(Action));
}

} // namespace nixd
