#include "nixd/AST/EvalAST.h"
#include "nixd/Expr/Expr.h"
#include "nixd/Nix/Option.h"
#include "nixd/Nix/Value.h"
#include "nixd/Server/Server.h"
#include "nixd/Support/Diagnostic.h"
#include "nixd/Support/Position.h"

#include "lspserver/Connection.h"
#include "lspserver/Logger.h"
#include "lspserver/Protocol.h"
#include "lspserver/SourceCode.h"

#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/shared.hh>

#include <llvm/ADT/StringRef.h>

#include <algorithm>
#include <exception>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace nixd {

void Controller::switchToEvaluator() {
  // initWorker();
  Role = ServerRole::Evaluator;
  EvalDiagnostic = mkOutNotifiction<ipc::Diagnostics>("nixd/ipc/diagnostic");

  // Registry.addMethod("nixd/ipc/textDocument/completion", this,
  //                    &Server::onEvalCompletion);

  // Registry.addMethod("nixd/ipc/textDocument/definition", this,
  //                    &Server::onEvalDefinition);

  evalInstallable();
  mkOutNotifiction<ipc::WorkerMessage>("nixd/ipc/finished")(
      ipc::WorkerMessage{WorkspaceVersion});
}

void Controller::evalInstallable() {
  assert(Role != ServerRole::Controller && "must be called in child workers.");
  auto Session = std::make_unique<IValueEvalSession>();

  auto I = Config.eval.target;
  auto Depth = Config.eval.depth;

  if (!I.empty())
    Session->parseArgs(I.nArgs());

  auto ILR = DraftMgr.injectFiles(Session->getState());

  ipc::Diagnostics Diagnostics;
  std::map<std::string, lspserver::PublishDiagnosticsParams> DiagMap;
  for (const auto &ErrInfo : ILR.InjectionErrors) {
    insertDiagnostic(*ErrInfo.Err, DiagMap,
                     decltype(DraftMgr)::decodeVersion(ErrInfo.Version));
  }
  Diagnostics.WorkspaceVersion = WorkspaceVersion;
  EvalDiagnostic(Diagnostics);
  try {
    if (!I.empty()) {
      Session->eval(I.installable, Depth);
      lspserver::log("evaluation done on worspace version: {0}",
                     WorkspaceVersion);
    }
  } catch (nix::BaseError &BE) {
    insertDiagnostic(BE, DiagMap);
  } catch (...) {
  }
  std::transform(DiagMap.begin(), DiagMap.end(),
                 std::back_inserter(Diagnostics.Params),
                 [](const auto &V) { return V.second; });
  EvalDiagnostic(Diagnostics);
  IER = std::make_unique<IValueEvalResult>(std::move(ILR.Forest),
                                           std::move(Session));
}

} // namespace nixd
