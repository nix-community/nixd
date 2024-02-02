/// \file
/// \brief Controller. The process interacting with users.
#include "nixd-config.h"

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Parse/Parser.h"

#include "lspserver/DraftStore.h"
#include "lspserver/LSPServer.h"
#include "lspserver/Path.h"
#include "lspserver/Protocol.h"
#include "lspserver/SourceCode.h"

#include <llvm/Support/JSON.h>

#include <sstream>

namespace {

using namespace lspserver;
using namespace llvm::json;

Position toLSPPosition(const nixf::Point &P) {
  return Position{static_cast<int>(P.line()), static_cast<int>(P.column())};
}

Range toLSPRange(const nixf::RangeTy &R) {
  return Range{toLSPPosition(R.begin()), toLSPPosition(R.end())};
}

int getLSPSeverity(nixf::Diagnostic::DiagnosticKind Kind) {
  switch (nixf::Diagnostic::severity(Kind)) {
  case nixf::Diagnostic::DS_Fatal:
  case nixf::Diagnostic::DS_Error:
    return 1;
  case nixf::Diagnostic::DS_Warning:
    return 2;
  }
  assert(false && "Invalid severity");
}

/// Holds analyzed information about a document.
///
/// TU stands for "Translation Unit".
class NixTU {
  std::vector<nixf::Diagnostic> Diagnostics;
  std::shared_ptr<nixf::Node> AST;

public:
  NixTU() = default;
  NixTU(std::vector<nixf::Diagnostic> Diagnostics,
        std::shared_ptr<nixf::Node> AST)
      : Diagnostics(std::move(Diagnostics)), AST(std::move(AST)) {}

  [[nodiscard]] const std::vector<nixf::Diagnostic> &diagnostics() const {
    return Diagnostics;
  }

  [[nodiscard]] const std::shared_ptr<nixf::Node> &ast() const { return AST; }
};

class Controller : public LSPServer {
  DraftStore Store;

  llvm::unique_function<void(const lspserver::PublishDiagnosticsParams &)>
      PublishDiagnostic;

  llvm::StringMap<NixTU> TUs;

  /// Action right after a document is added (including updates).
  void actOnDocumentAdd(PathRef File, std::optional<int64_t> Version) {
    auto Draft = Store.getDraft(File);
    assert(Draft && "Added document is not in the store?");
    std::vector<nixf::Diagnostic> Diagnostics;
    std::shared_ptr<nixf::Node> AST =
        nixf::parse(*Draft->Contents, Diagnostics);
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

      LSPDiags.emplace_back(Diagnostic{
          .range = toLSPRange(D.range()),
          .severity = getLSPSeverity(D.kind()),
          .code = D.sname(),
          .source = "nixf",
          .message = Message,
      });

      for (const nixf::Note &N : D.notes()) {
        LSPDiags.emplace_back(Diagnostic{
            .range = toLSPRange(N.range()),
            .severity = 3,
            .source = "nixf",
            .message = N.format(),
        });
      }
    }
    PublishDiagnostic({
        .uri = URIForFile::canonicalize(File, File),
        .diagnostics = std::move(LSPDiags),
        .version = Version,
    });
    TUs[File] = NixTU{std::move(Diagnostics), std::move(AST)};
  }

  void removeDocument(PathRef File) { Store.removeDraft(File); }

  void onInitialize( // NOLINT(readability-convert-member-functions-to-static)
      [[maybe_unused]] const InitializeParams &Params, Callback<Value> Reply) {

    Object ServerCaps{
        {{"textDocumentSync",
          llvm::json::Object{
              {"openClose", true},
              {"change", (int)TextDocumentSyncKind::Incremental},
              {"save", true},
          }},
         {
             "codeActionProvider",
             Object{
                 {"codeActionKinds", Array{CodeAction::QUICKFIX_KIND}},
                 {"resolveProvider", false},
             },
         }},
    };

    Object Result{{
        {"serverInfo",
         Object{
             {"name", "nixd"},
             {"version", NIXD_VERSION},
         }},
        {"capabilities", std::move(ServerCaps)},
    }};

    Reply(std::move(Result));

    PublishDiagnostic = mkOutNotifiction<PublishDiagnosticsParams>(
        "textDocument/publishDiagnostics");
  }
  void onInitialized([[maybe_unused]] const InitializedParams &Params) {}

  void onDocumentDidOpen(const DidOpenTextDocumentParams &Params) {
    PathRef File = Params.textDocument.uri.file();
    const std::string &Contents = Params.textDocument.text;
    std::optional<int64_t> Version = Params.textDocument.version;
    Store.addDraft(File, DraftStore::encodeVersion(Version), Contents);
    actOnDocumentAdd(File, Version);
  }

  void onDocumentDidChange(const DidChangeTextDocumentParams &Params) {
    PathRef File = Params.textDocument.uri.file();
    auto Code = Store.getDraft(File);
    if (!Code) {
      log("Trying to incrementally change non-added document: {0}", File);
      return;
    }
    std::string NewCode(*Code->Contents);
    for (const auto &Change : Params.contentChanges) {
      if (auto Err = applyChange(NewCode, Change)) {
        // If this fails, we are most likely going to be not in sync anymore
        // with the client.  It is better to remove the draft and let further
        // operations fail rather than giving wrong results.
        removeDocument(File);
        elog("Failed to update {0}: {1}", File, std::move(Err));
        return;
      }
    }
    std::optional<int64_t> Version = Params.textDocument.version;
    Store.addDraft(File, DraftStore::encodeVersion(Version), NewCode);
    actOnDocumentAdd(File, Version);
  }

  void onDocumentDidClose(const DidCloseTextDocumentParams &Params) {
    PathRef File = Params.textDocument.uri.file();
    removeDocument(File);
  }

  void onCodeAction(const CodeActionParams &Params,
                    Callback<std::vector<CodeAction>> Reply) {
    PathRef File = Params.textDocument.uri.file();
    Range Range = Params.range;
    const std::vector<nixf::Diagnostic> &Diagnostics = TUs[File].diagnostics();
    std::vector<CodeAction> Actions;
    Actions.reserve(Diagnostics.size());
    for (const nixf::Diagnostic &D : Diagnostics) {
      // Skip diagnostics that are not in the range.
      if (!Range.contains(toLSPRange(D.range())))
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

public:
  Controller(std::unique_ptr<InboundPort> In, std::unique_ptr<OutboundPort> Out)
      : LSPServer(std::move(In), std::move(Out)) {

    // Life Cycle
    Registry.addMethod("initialize", this, &Controller::onInitialize);
    Registry.addNotification("initialized", this, &Controller::onInitialized);

    // Text Document Synchronization
    Registry.addNotification("textDocument/didOpen", this,
                             &Controller::onDocumentDidOpen);
    Registry.addNotification("textDocument/didChange", this,
                             &Controller::onDocumentDidChange);

    Registry.addNotification("textDocument/didClose", this,
                             &Controller::onDocumentDidClose);

    // Language Features
    Registry.addMethod("textDocument/codeAction", this,
                       &Controller::onCodeAction);
  }
};

} // namespace

namespace nixd {
void runController(std::unique_ptr<InboundPort> In,
                   std::unique_ptr<OutboundPort> Out) {
  Controller C(std::move(In), std::move(Out));
  C.run();
}
} // namespace nixd
