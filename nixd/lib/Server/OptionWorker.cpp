#include "nixd/Server/OptionWorker.h"
#include "nixd/Nix/Init.h"
#include "nixd/Nix/Option.h"
#include "nixd/Nix/Value.h"
#include "nixd/Sema/CompletionBuilder.h"
#include "nixd/Server/ConfigSerialization.h"
#include "nixd/Server/IPCSerialization.h"
#include "nixd/Support/Diagnostic.h"
#include "nixd/Support/ReplyRAII.h"

#include <mutex>

namespace nixd {

OptionWorker::OptionWorker(std::unique_ptr<lspserver::InboundPort> In,
                           std::unique_ptr<lspserver::OutboundPort> Out)
    : lspserver::LSPServer(std::move(In), std::move(Out)) {

  initEval();

  Registry.addNotification("nixd/ipc/evalOptionSet", this,
                           &OptionWorker::onEvalOptionSet);
  Registry.addMethod("nixd/ipc/textDocument/completion/options", this,
                     &OptionWorker::onCompletion);
  Registry.addMethod("nixd/ipc/option/textDocument/declaration", this,
                     &OptionWorker::onDeclaration);
}

void OptionWorker::onDeclaration(
    const ipc::AttrPathParams &Params,
    lspserver::Callback<lspserver::Location> Reply) {
  using namespace lspserver;
  ReplyRAII<Location> RR(std::move(Reply));
  if (!OptionAttrSet)
    return;
  if (OptionAttrSet->type() != nix::ValueType::nAttrs)
    return;

  try {
    auto Path = Params.Path;
    Path.emplace_back("declarations");

    auto &State = *OptionIES->getState();

    auto VDecl = selectAttrPath(State, *OptionAttrSet, Path);
    State.forceValue(VDecl, nix::noPos);

    // declarations should be a list containing file paths
    if (!VDecl.isList())
      return;

    for (auto *VFile : VDecl.listItems()) {
      State.forceValue(*VFile, nix::noPos);
      if (VFile->type() == nix::ValueType::nString) {
        auto File = VFile->str();
        Location L;
        L.uri = URIForFile::canonicalize(File, File);

        // Where is the range?
        L.range.start = {0, 0};
        L.range.end = {0, 0};

        RR.Response = L;
        return;
      }
    }

  } catch (std::exception &E) {
    RR.Response = error(stripANSI(E.what()));
  } catch (...) {
  }
}

void OptionWorker::onEvalOptionSet(
    const configuration::TopLevel::Options &Config) {
  if (!Config.enable)
    return;
  if (Config.target.empty()) {
    lspserver::elog(
        "enabled options completion, but the target set is unspecified!");
    return;
  }
  try {
    auto I = Config.target;
    auto SessionOption = std::make_unique<IValueEvalSession>();
    SessionOption->parseArgs(I.nArgs());
    OptionAttrSet = SessionOption->eval(I.installable);
    OptionIES = std::move(SessionOption);
    lspserver::log("options are ready");
  } catch (std::exception &E) {
    lspserver::elog("exception {0} encountered while evaluating options",
                    stripANSI(E.what()));
    OptionIES = nullptr;
    OptionAttrSet = nullptr;
  }
}

void OptionWorker::onCompletion(const ipc::AttrPathParams &Params,
                                lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  ReplyRAII<CompletionList> RR(std::move(Reply));

  if (!OptionAttrSet)
    return;

  CompletionBuilder Builder;
  Builder.addOption(*OptionIES->getState(), *OptionAttrSet, Params.Path);
  RR.Response = Builder.getResult();
}

} // namespace nixd
