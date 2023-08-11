#include "nixd/Server/EvalWorker.h"
#include "nixd/Nix/Init.h"
#include "nixd/Nix/Value.h"
#include "nixd/Sema/CompletionBuilder.h"
#include "nixd/Server/EvalDraftStore.h"
#include "nixd/Server/IPC.h"
#include "nixd/Support/String.h"

#include "lspserver/LSPServer.h"

#include <llvm/ADT/StringRef.h>

namespace nixd {

EvalWorker::EvalWorker(std::unique_ptr<lspserver::InboundPort> In,
                       std::unique_ptr<lspserver::OutboundPort> Out)
    : lspserver::LSPServer(std::move(In), std::move(Out)) {

  initEval();

  NotifyDiagnostics = mkOutNotifiction<ipc::Diagnostics>("nixd/ipc/diagnostic");

  Registry.addNotification("nixd/ipc/eval", this, &EvalWorker::onEval);
  Registry.addNotification("nixd/ipc/textDocument/didOpen", this,
                           &EvalWorker::onDocumentDidOpen);
  Registry.addMethod("nixd/ipc/textDocument/hover", this, &EvalWorker::onHover);
  Registry.addMethod("nixd/ipc/textDocument/completion", this,
                     &EvalWorker::onCompletion);
  Registry.addMethod("nixd/ipc/textDocument/definition", this,
                     &EvalWorker::onDefinition);
}

void EvalWorker::onDocumentDidOpen(
    const lspserver::DidOpenTextDocumentParams &Params) {
  lspserver::PathRef File = Params.textDocument.uri.file();

  const std::string &Contents = Params.textDocument.text;

  auto Version = EvalDraftStore::encodeVersion(Params.textDocument.version);

  DraftMgr.addDraft(File, Version, Contents);
}

void EvalWorker::onEval(const ipc::EvalParams &Params) {
  auto Session = std::make_unique<IValueEvalSession>();

  auto I = Params.Eval.target;
  auto Depth = Params.Eval.depth;

  if (!I.empty())
    Session->parseArgs(I.nArgs());

  auto ILR = DraftMgr.injectFiles(Session->getState());

  std::map<std::string, lspserver::PublishDiagnosticsParams> DiagMap;
  for (const auto &ErrInfo : ILR.InjectionErrors) {
    insertDiagnostic(*ErrInfo.Err, DiagMap,
                     lspserver::DraftStore::decodeVersion(ErrInfo.Version));
  }
  try {
    if (!I.empty()) {
      Session->eval(I.installable, Depth);
      lspserver::log("finished evaluation");
    }
  } catch (nix::BaseError &BE) {
    insertDiagnostic(BE, DiagMap);
  } catch (...) {
  }

  ipc::Diagnostics Diagnostics;
  std::transform(DiagMap.begin(), DiagMap.end(),
                 std::back_inserter(Diagnostics.Params),
                 [](const auto &V) { return V.second; });

  Diagnostics.WorkspaceVersion = Params.WorkspaceVersion;
  NotifyDiagnostics(Diagnostics);
  IER = std::make_unique<IValueEvalResult>(std::move(ILR.Forest),
                                           std::move(Session));
}

void EvalWorker::onDefinition(
    const lspserver::TextDocumentPositionParams &Params,
    lspserver::Callback<lspserver::Location> Reply) {
  using namespace lspserver;
  withAST<Location>(
      Params.textDocument.uri.file().str(),
      ReplyRAII<Location>(std::move(Reply)),
      [Params, this](const nix::ref<EvalAST> &AST, ReplyRAII<Location> &&RR) {
        auto State = IER->Session->getState();

        const auto *Node = AST->lookupContainMin(Params.position);
        if (!Node)
          return;

        // If the expression evaluates to a "derivation", try to bring our user
        // to the location which defines the package.
        auto OpV = AST->getValueEval(Node, *State);
        if (!OpV)
          return;

        auto V = *OpV;
        if (isDerivation(*State, V)) {
          if (auto OpPosStr = attrPathStr(*State, V, "meta.position")) {
            auto [Path, LineStr] = llvm::StringRef(*OpPosStr).split(':');
            int Line;
            if (LLVM_LIKELY(!LineStr.getAsInteger(10, Line))) {
              lspserver::Position Position{.line = Line, .character = 0};
              RR.Response = Location{URIForFile::canonicalize(Path, Path),
                                     {Position, Position}};
              return;
            }
          }
        } else {
          // There is a value avaiable, this might be useful for locations
          auto P = V.determinePos(nix::noPos);
          if (P != nix::noPos) {
            auto Pos = State->positions[P];
            if (auto *SourcePath = std::get_if<nix::SourcePath>(&Pos.origin)) {
              auto Path = SourcePath->to_string();
              lspserver::Position Position = toLSPPos(State->positions[P]);
              RR.Response = Location{URIForFile::canonicalize(Path, Path),
                                     {Position, Position}};
              return;
            }
          }
        }
      });
}

void EvalWorker::onHover(const lspserver::TextDocumentPositionParams &Params,
                         lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  withAST<Hover>(
      Params.textDocument.uri.file().str(), ReplyRAII<Hover>(std::move(Reply)),
      [Params, this](const nix::ref<EvalAST> &AST, ReplyRAII<Hover> &&RR) {
        const auto *Node = AST->lookupContainMin(Params.position);
        if (!Node)
          return;
        const auto *ExprName = getExprName(Node);
        RR.Response = Hover{{MarkupKind::Markdown, ""}, std::nullopt};
        auto &HoverText = RR.Response->contents.value;
        if (auto OpV = AST->getValueEval(Node, *IER->Session->getState())) {
          std::stringstream Res{};
          PrintDepth = 3;
          OpV->print(IER->Session->getState()->symbols, Res);
          HoverText =
              llvm::formatv("## {0} \n Value: `{1}`", ExprName, Res.str());
        } else {
          // No such value, just reply dummy item
          std::stringstream NodeOut;
          Node->show(IER->Session->getState()->symbols, NodeOut);
          lspserver::vlog("no associated value on node {0}!", NodeOut.str());
          HoverText = llvm::formatv("`{0}`", ExprName);
        }
        truncateString(HoverText, 300);
      });
}

void EvalWorker::onCompletion(const lspserver::CompletionParams &Params,
                              lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  auto Action = [Params, this](const nix::ref<EvalAST> &AST,
                               ReplyRAII<CompletionList> &&RR) {
    auto State = IER->Session->getState();
    CompletionBuilder Builder;
    // Lookup an AST node, and get it's 'Env' after evaluation
    lspserver::log("current trigger character is {0}",
                   Params.context.triggerCharacter);

    if (Params.context.triggerCharacter == ".") {
      Builder.addAttrFields(*AST, Params.position, *State);
    } else {
      const auto *Node = AST->lookupContainMin(Params.position);
      Builder.addSymbols(*AST, Node);
      Builder.addLambdaFormals(*AST, *State, Node);
      Builder.addEnv(*AST, *State, Node);
      Builder.addStaticEnv(State->symbols, *State->staticBaseEnv);
    }
    if (!Builder.getResult().items.empty())
      RR.Response = Builder.getResult();
  };

  withAST<CompletionList>(Params.textDocument.uri.file().str(),
                          ReplyRAII<CompletionList>(std::move(Reply)),
                          std::move(Action));
}

} // namespace nixd
