#include "nixd/Diagnostic.h"
#include "nixd/Server.h"
#include "nixd/nix/Option.h"
#include "nixd/nix/Value.h"

#include <mutex>

namespace nixd {

void Server::forkOptionWorker() {
  std::lock_guard _(OptionWorkerLock);
  forkWorker(
      [this]() {
        switchToOptionProvider();
        Registry.addMethod("nixd/ipc/textDocument/completion/options", this,
                           &Server::onOptionCompletion);
        for (auto &W : OptionWorkers) {
          W->Pid.release();
        }
      },
      OptionWorkers, 1);
}

void Server::onOptionDeclaration(
    const ipc::AttrPathParams &Params,
    lspserver::Callback<lspserver::Location> Reply) {
  assert(Role == ServerRole::OptionProvider &&
         "option declaration should be calculated in option worker!");
  using namespace lspserver;
  ReplyRAII<Location> RR(std::move(Reply));
  if (OptionAttrSet->type() != nix::ValueType::nAttrs)
    return;

  try {
    nix::Value *V = OptionAttrSet;
    if (!Params.Path.empty()) {
      auto &Bindings(*OptionIES->getState()->allocBindings(0));
      V = nix::findAlongAttrPath(*OptionIES->getState(), Params.Path, Bindings,
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

void Server::switchToOptionProvider() {
  initWorker();
  Role = ServerRole::OptionProvider;

  if (!Config.options.has_value() || !Config.options->enable.value_or(false) ||
      !Config.options->target.has_value() || Config.options->target->empty())
    return;
  try {
    auto I = Config.options->target.value();
    auto SessionOption = std::make_unique<IValueEvalSession>();
    SessionOption->parseArgs(I.ndArgs());
    OptionAttrSet = SessionOption->eval(I.dInstallable());
    OptionIES = std::move(SessionOption);
    lspserver::log("options are ready");
  } catch (std::exception &E) {
    lspserver::elog("exception {0} encountered while evaluating options",
                    stripANSI(E.what()));
  }
}

void Server::onOptionCompletion(const ipc::AttrPathParams &Params,
                                lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  using namespace nix::nixd;
  ReplyRAII<CompletionList> RR(std::move(Reply));

  try {
    CompletionList List;
    List.isIncomplete = false;
    List.items = decltype(CompletionList::items){};

    auto &Items = List.items;

    if (OptionAttrSet->type() == nix::ValueType::nAttrs) {
      nix::Value *V = OptionAttrSet;
      if (!Params.Path.empty()) {
        auto &Bindings(*OptionIES->getState()->allocBindings(0));
        V = nix::findAlongAttrPath(*OptionIES->getState(), Params.Path,
                                   Bindings, *OptionAttrSet)
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
