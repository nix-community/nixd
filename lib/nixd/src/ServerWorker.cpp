#include "nixd/AST.h"
#include "nixd/Diagnostic.h"
#include "nixd/Expr.h"
#include "nixd/Server.h"
#include "nixd/nix/Option.h"
#include "nixd/nix/Value.h"

#include "lspserver/Connection.h"
#include "lspserver/Logger.h"
#include "lspserver/Protocol.h"
#include "lspserver/SourceCode.h"

#include <nix/attr-path.hh>
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/globals.hh>
#include <nix/input-accessor.hh>
#include <nix/nixexpr.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <nix/symbol-table.hh>
#include <nix/util.hh>
#include <nix/value.hh>

#include <llvm/ADT/StringRef.h>

#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace nixd {

CompletionHelper::Items
CompletionHelper::fromStaticEnv(const nix::SymbolTable &STable,
                                const nix::StaticEnv &SEnv) {
  Items Result;
  for (auto [Symbol, Displ] : SEnv.vars) {
    std::string Name = STable[Symbol];
    if (Name.starts_with("__"))
      continue;

    // Variables in static envs, let's mark it as "Constant".
    Result.emplace_back(lspserver::CompletionItem{
        .label = Name, .kind = lspserver::CompletionItemKind::Constant});
  }
  return Result;
}

CompletionHelper::Items
CompletionHelper::fromEnvWith(const nix::SymbolTable &STable,
                              const nix::Env &NixEnv) {
  Items Result;
  if (NixEnv.type == nix::Env::HasWithAttrs) {
    for (const auto &SomeAttr : *NixEnv.values[0]->attrs) {
      std::string Name = STable[SomeAttr.name];
      Result.emplace_back(lspserver::CompletionItem{
          .label = Name, .kind = lspserver::CompletionItemKind::Variable});
    }
  }
  return Result;
}

CompletionHelper::Items
CompletionHelper::fromEnvRecursive(const nix::SymbolTable &STable,
                                   const nix::StaticEnv &SEnv,
                                   const nix::Env &NixEnv) {

  Items Result;
  if ((SEnv.up != nullptr) && (NixEnv.up != nullptr)) {
    Items Inherited = fromEnvRecursive(STable, *SEnv.up, *NixEnv.up);
    Result.insert(Result.end(), Inherited.begin(), Inherited.end());
  }

  auto StaticEnvItems = fromStaticEnv(STable, SEnv);
  Result.insert(Result.end(), StaticEnvItems.begin(), StaticEnvItems.end());

  auto EnvItems = fromEnvWith(STable, NixEnv);
  Result.insert(Result.end(), EnvItems.begin(), EnvItems.end());

  return Result;
}

void Server::initWorker() {
  assert(Role == ServerRole::Controller &&
         "Must switch from controller's fork!");
  nix::initNix();
  nix::initLibStore();
  nix::initPlugins();
  nix::initGC();

  /// Basically communicate with the controller in standard mode. because we do
  /// not support "lit-test" outbound port.
  switchStreamStyle(lspserver::JSONStreamStyle::Standard);
}

void Server::switchToEvaluator(lspserver::PathRef File) {
  initWorker();
  Role = ServerRole::Evaluator;
  WorkerDiagnostic = mkOutNotifiction<ipc::Diagnostics>("nixd/ipc/diagnostic");
  evalInstallable(File, Config.getEvalDepth());
}

void Server::switchToOptionProvider() {
  initWorker();
  Role = ServerRole::OptionProvider;

  if (!Config.options.has_value() || !Config.options->enable.value_or(false) ||
      !Config.options->target.has_value())
    return;
  try {
    auto [Args, Installable] =
        configuration::TopLevel::getInstallable(Config.options->target.value());
    auto SessionOption = std::make_unique<IValueEvalSession>();
    SessionOption->parseArgs(Args);
    OptionAttrSet = SessionOption->eval(Installable);
    OptionIES = std::move(SessionOption);
    lspserver::log("options are ready");
  } catch (std::exception &E) {
    lspserver::elog("exception {0} encountered while evaluating options",
                    stripANSI(E.what()));
  }
}

lspserver::PublishDiagnosticsParams
Server::diagNixError(lspserver::PathRef Path, const nix::BaseError &NixErr,
                     std::optional<int64_t> Version) {
  using namespace lspserver;
  auto DiagVec = mkDiagnostics(NixErr);
  URIForFile Uri = URIForFile::canonicalize(Path, Path);
  PublishDiagnosticsParams Notification;
  Notification.uri = Uri;
  Notification.diagnostics = DiagVec;
  Notification.version = Version;
  return Notification;
}

void Server::evalInstallable(lspserver::PathRef File, int Depth = 0) {
  assert(Role != ServerRole::Controller && "must be called in child workers.");
  auto Session = std::make_unique<IValueEvalSession>();

  nix::Strings Args;
  std::string Installable;
  if (!Config.eval.has_value() || !Config.eval->target) {
    Args = {"--file", std::string(File)};
    Installable = "";
  } else {
    std::tie(Args, Installable) =
        configuration::TopLevel::getInstallable(Config.eval->target.value());
  }

  Session->parseArgs(Args);

  auto ILR = DraftMgr.injectFiles(Session->getState());

  ipc::Diagnostics Diagnostics;
  for (const auto &[ErrObject, ErrInfo] : ILR.InjectionErrors) {
    Diagnostics.Params.emplace_back(
        diagNixError(ErrInfo.ActiveFile, *ErrObject,
                     decltype(DraftMgr)::decodeVersion(ErrInfo.Version)));
  }
  Diagnostics.WorkspaceVersion = WorkspaceVersion;
  WorkerDiagnostic(Diagnostics);
  try {
    Session->eval(Installable, Depth);
    lspserver::log("evaluation done on worspace version: {0}",
                   WorkspaceVersion);
  } catch (nix::BaseError &BE) {
    Diagnostics.Params.emplace_back(diagNixError(File, BE, std::nullopt));
  } catch (...) {
  }
  WorkerDiagnostic(Diagnostics);
  IER = std::make_unique<IValueEvalResult>(std::move(ILR.Forest),
                                           std::move(Session));
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

void Server::onEvalDefinition(
    const lspserver::TextDocumentPositionParams &Params,
    lspserver::Callback<lspserver::Location> Reply) {
  using namespace lspserver;
  withAST<Location>(
      Params.textDocument.uri.file().str(),
      ReplyRAII<Location>(std::move(Reply)),
      [Params, this](const nix::ref<EvalAST> &AST, ReplyRAII<Location> &&RR) {
        auto State = IER->Session->getState();

        auto *Node = AST->lookupPosition(Params.position);

        // If the expression evaluates to a "derivation", try to bring our user
        // to the location which defines the package.
        try {
          auto V = AST->getValue(Node);
          if (nix::nixd::isDerivation(*State, V)) {
            if (auto S = nix::nixd::attrPathStr(*State, V, "meta.position")) {
              llvm::StringRef PositionStr = S.value();
              auto [Path, LineStr] = PositionStr.split(':');
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
              if (auto *SourcePath =
                      std::get_if<nix::SourcePath>(&Pos.origin)) {
                auto Path = SourcePath->to_string();
                lspserver::Position Position =
                    translatePosition(State->positions[P]);
                RR.Response = Location{URIForFile::canonicalize(Path, Path),
                                       {Position, Position}};
                return;
              }
            }
          }
        } catch (...) {
          lspserver::vlog("no associated value on this expression");
        }

        // Otherwise, we try to find the location binds to the variable.
        if (const auto *EVar = dynamic_cast<const nix::ExprVar *>(Node)) {
          if (EVar->fromWith)
            return;
          auto PIdx = AST->definition(EVar);
          if (PIdx == nix::noPos)
            return;

          auto Position = translatePosition(State->positions[PIdx]);
          RR.Response = Location{Params.textDocument.uri, {Position, Position}};
          return;
        }
        RR.Response = error("requested expression is not an ExprVar.");
        return;
      });
}

void Server::onEvalHover(const lspserver::TextDocumentPositionParams &Params,
                         lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  withAST<Hover>(
      Params.textDocument.uri.file().str(), ReplyRAII<Hover>(std::move(Reply)),
      [Params, this](const nix::ref<EvalAST> &AST, ReplyRAII<Hover> &&RR) {
        auto *Node = AST->lookupPosition(Params.position);
        const auto *ExprName = getExprName(Node);
        RR.Response = Hover{{MarkupKind::Markdown, ""}, std::nullopt};
        auto &HoverText = RR.Response->contents.value;
        try {
          auto Value = AST->getValue(Node);
          std::stringstream Res{};
          Value.print(IER->Session->getState()->symbols, Res);
          HoverText =
              llvm::formatv("## {0} \n Value: `{1}`", ExprName, Res.str());
        } catch (const std::out_of_range &) {
          // No such value, just reply dummy item
          std::stringstream NodeOut;
          Node->show(IER->Session->getState()->symbols, NodeOut);
          lspserver::vlog("no associated value on node {0}!", NodeOut.str());
          HoverText = llvm::formatv("`{0}`", ExprName);
        }
      });
}

void Server::onOptionCompletion(const ipc::AttrPathParams &Params,
                                lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  using namespace nix::nixd;
  ReplyRAII<CompletionList> RR(std::move(Reply));

  try {
    CompletionList List;
    List.isIncomplete = false;
    List.items = CompletionHelper::Items{};

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

void Server::onEvalCompletion(const lspserver::CompletionParams &Params,
                              lspserver::Callback<llvm::json::Value> Reply) {
  using namespace lspserver;
  auto Action = [Params, this](const nix::ref<EvalAST> &AST,
                               ReplyRAII<CompletionList> &&RR) {
    auto State = IER->Session->getState();
    // Lookup an AST node, and get it's 'Env' after evaluation
    CompletionHelper::Items Items;
    lspserver::log("current trigger character is {0}",
                   Params.context.triggerCharacter);

    if (Params.context.triggerCharacter == ".") {
      auto *Node = AST->lookupPosition(Params.position);
      try {
        auto Value = AST->getValue(Node);
        if (Value.type() == nix::ValueType::nAttrs) {
          // Traverse attribute bindings
          for (auto Binding : *Value.attrs) {
            Items.emplace_back(
                CompletionItem{.label = State->symbols[Binding.name],
                               .kind = CompletionItemKind::Field});
          }
        }
      } catch (std::out_of_range &) {
        RR.Response = error("no associated value on requested attrset.");
        return;
      }
    } else {
      try {
        auto *Node = AST->lookupPosition(Params.position);
        const auto *ExprEnv = AST->getEnv(Node);

        Items = CompletionHelper::fromEnvRecursive(
            State->symbols, *State->staticBaseEnv, *ExprEnv);
      } catch (std::out_of_range &) {
        Items = CompletionHelper::fromStaticEnv(State->symbols,
                                                *State->staticBaseEnv);
      }
    }
    // Make the response.
    CompletionList List;
    List.isIncomplete = false;
    List.items = Items;
    RR.Response = List;
  };

  withAST<CompletionList>(Params.textDocument.uri.file().str(),
                          ReplyRAII<CompletionList>(std::move(Reply)),
                          std::move(Action));
}

} // namespace nixd
