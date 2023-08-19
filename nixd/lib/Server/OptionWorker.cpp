#include "nixd/Server/OptionWorker.h"
#include "nixd/Nix/Init.h"
#include "nixd/Nix/Option.h"
#include "nixd/Nix/Value.h"
#include "nixd/Server/ConfigSerialization.h"
#include "nixd/Server/IPCSerialization.h"
#include "nixd/Support/Diagnostic.h"
#include "nixd/Support/ReplyRAII.h"

#include <boost/algorithm/string.hpp>

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
  using boost::algorithm::join;
  ReplyRAII<Location> RR(std::move(Reply));
  if (!OptionAttrSet)
    return;
  if (OptionAttrSet->type() != nix::ValueType::nAttrs)
    return;

  try {
    nix::Value *V = OptionAttrSet;
    if (!Params.Path.empty()) {
      auto AttrPath = join(Params.Path, ".");
      auto &Bindings(*OptionIES->getState()->allocBindings(0));
      V = nix::findAlongAttrPath(*OptionIES->getState(), AttrPath, Bindings,
                                 *OptionAttrSet)
              .first;
    }

    auto &State = *OptionIES->getState();
    State.forceValue(*V, nix::noPos);

    auto *VDecl = nix::findAlongAttrPath(State, "declarations",
                                         *State.allocBindings(0), *V)
                      .first;

    State.forceValue(*VDecl, nix::noPos);

    // declarations should be a list containing file paths
    if (!VDecl->isList())
      return;

    for (auto *VFile : VDecl->listItems()) {
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
  using boost::algorithm::join;
  ReplyRAII<CompletionList> RR(std::move(Reply));

  if (!OptionAttrSet)
    return;

  try {
    CompletionList List;
    List.isIncomplete = false;
    List.items = decltype(CompletionList::items){};

    auto &Items = List.items;

    if (OptionAttrSet->type() == nix::ValueType::nAttrs) {
      nix::Value *V = OptionAttrSet;
      if (!Params.Path.empty()) {
        auto AttrPath = join(Params.Path, ".");
        auto &Bindings(*OptionIES->getState()->allocBindings(0));
        V = nix::findAlongAttrPath(*OptionIES->getState(), AttrPath, Bindings,
                                   *OptionAttrSet)
                .first;
      }

      OptionIES->getState()->forceValue(*V, nix::noPos);

      if (V->type() != nix::ValueType::nAttrs)
        return;

      auto &State = *OptionIES->getState();

      if (isOption(State, *V))
        return;

      for (auto Attr : *V->attrs) {
        if (isOption(State, *Attr.value)) {
          auto OI = optionInfo(State, *Attr.value);
          Items.emplace_back(CompletionItem{
              .label = State.symbols[Attr.name],
              .kind = CompletionItemKind::Constructor,
              .detail = OI.Type.value_or(""),
              .documentation =
                  MarkupContent{MarkupKind::Markdown, OI.mdDoc()}});
        } else {
          Items.emplace_back(
              CompletionItem{.label = OptionIES->getState()->symbols[Attr.name],
                             .kind = CompletionItemKind::Class,
                             .detail = "{...}",
                             .documentation = std::nullopt});
        }
      }
      RR.Response = std::move(List);
    }
  } catch (std::exception &E) {
    RR.Response = error(E.what());
  }
}

} // namespace nixd
