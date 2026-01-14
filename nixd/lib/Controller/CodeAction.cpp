/// \file
/// \brief Implementation of [Code Action].
/// [Code Action]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_codeAction

#include "CheckReturn.h"
#include "Convert.h"

#include "CodeActions/AttrName.h"
#include "CodeActions/ExtractToFile.h"
#include "CodeActions/FlattenAttrs.h"
#include "CodeActions/JsonToNix.h"
#include "CodeActions/NoogleDoc.h"
#include "CodeActions/PackAttrs.h"
#include "CodeActions/RewriteString.h"

#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>
#include <llvm/Support/JSON.h>

namespace nixd {

using namespace llvm::json;
using namespace lspserver;

void Controller::onCodeAction(const lspserver::CodeActionParams &Params,
                              Callback<std::vector<CodeAction>> Reply) {
  using CheckTy = std::vector<CodeAction>;
  std::string File(Params.textDocument.uri.file());
  Range Range = Params.range;
  auto Action = [Reply = std::move(Reply), File, Range, this]() mutable {
    return Reply([&]() -> llvm::Expected<CheckTy> {
      const auto TU = CheckDefault(getTU(File));

      const auto &Diagnostics = TU->diagnostics();
      auto Actions = std::vector<CodeAction>();
      Actions.reserve(Diagnostics.size());
      std::string FileURI = URIForFile::canonicalize(File, File).uri();

      for (const nixf::Diagnostic &D : Diagnostics) {
        auto DRange = toLSPRange(TU->src(), D.range());
        if (!Range.overlap(DRange))
          continue;

        // Add fixes.
        for (const nixf::Fix &F : D.fixes()) {
          std::vector<TextEdit> Edits;
          Edits.reserve(F.edits().size());
          for (const nixf::TextEdit &TE : F.edits()) {
            Edits.emplace_back(TextEdit{
                .range = toLSPRange(TU->src(), TE.oldRange()),
                .newText = std::string(TE.newText()),
            });
          }
          using Changes = std::map<std::string, std::vector<TextEdit>>;
          WorkspaceEdit WE{.changes = Changes{
                               {FileURI, std::move(Edits)},
                           }};
          Actions.emplace_back(CodeAction{
              .title = F.message(),
              .kind = std::string(CodeAction::QUICKFIX_KIND),
              .edit = std::move(WE),
          });
        }
      }

      // Add refactoring code actions based on cursor position
      if (TU->ast() && TU->parentMap()) {
        nixf::PositionRange NixfRange = toNixfRange(Range);
        if (const nixf::Node *N = TU->ast()->descend(NixfRange)) {
          addAttrNameActions(*N, *TU->parentMap(), FileURI, TU->src(), Actions);
          addFlattenAttrsAction(*N, *TU->parentMap(), FileURI, TU->src(),
                                Actions);
          addPackAttrsAction(*N, *TU->parentMap(), FileURI, TU->src(), Actions);
          addNoogleDocAction(*N, *TU->parentMap(), Actions);
          addRewriteStringAction(*N, *TU->parentMap(), FileURI, TU->src(),
                                 Actions);

          // Extract to file requires variable lookup analysis
          if (TU->variableLookup()) {
            addExtractToFileAction(*N, *TU->parentMap(), *TU->variableLookup(),
                                   FileURI, TU->src(), Actions);
          }
        }
      }

      // Selection-based actions (work on arbitrary text, not AST nodes)
      addJsonToNixAction(TU->src(), Range, FileURI, Actions);

      return Actions;
    }());
  };
  boost::asio::post(Pool, std::move(Action));
}

void Controller::onCodeActionResolve(const lspserver::CodeAction &Params,
                                     Callback<CodeAction> Reply) {
  auto Action = [Reply = std::move(Reply), Params, this]() mutable {
    // Check if this is a Noogle documentation action
    if (Params.data) {
      const auto *DataObj = Params.data->getAsObject();
      if (DataObj) {
        auto NoogleUrl = DataObj->getString("noogleUrl");
        if (NoogleUrl) {
          // Call window/showDocument to open the URL in external browser
          ShowDocumentParams ShowParams;
          ShowParams.externalUri = NoogleUrl->str();
          ShowParams.external = true;

          ShowDocument(
              ShowParams, [](llvm::Expected<ShowDocumentResult> Result) {
                if (!Result) {
                  lspserver::elog("Failed to open Noogle documentation: {0}",
                                  Result.takeError());
                }
              });
        }
      }
    }

    // Return the resolved code action (unchanged for Noogle actions since
    // the work is done via showDocument)
    Reply(Params);
  };
  boost::asio::post(Pool, std::move(Action));
}

} // namespace nixd
