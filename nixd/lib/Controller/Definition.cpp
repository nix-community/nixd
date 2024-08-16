/// \file
/// \brief Implementation of [Go to Definition]
/// [Go to Definition]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_definition

#include "Definition.h"
#include "AST.h"
#include "Convert.h"

#include "nixd/Controller/Controller.h"
#include "nixd/Protocol/AttrSet.h"

#include "lspserver/Protocol.h"

#include <boost/asio/post.hpp>

#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>

#include <nixf/Basic/Nodes/Attrs.h>
#include <nixf/Basic/Nodes/Basic.h>
#include <nixf/Basic/Nodes/Expr.h>
#include <nixf/Sema/ParentMap.h>
#include <nixf/Sema/VariableLookup.h>

#include <exception>
#include <semaphore>

using namespace nixd;
using namespace nixd::idioms;
using namespace nixf;
using namespace lspserver;
using namespace llvm;

using LookupResult = VariableLookupAnalysis::LookupResult;
using ResultKind = VariableLookupAnalysis::LookupResultKind;
using Locations = std::vector<Location>;

namespace {

const Definition *findSelfDefinition(const Node &N,
                                     const ParentMapAnalysis &PMA,
                                     const VariableLookupAnalysis &VLA) {
  // If "N" is a definition itself, just return it.
  if (const Definition *Def = VLA.toDef(N))
    return Def;

  // If N is inside an attrset, it maybe an "AttrName", let's look for it.
  const Node *Parent = PMA.query(N);
  if (Parent && Parent->kind() == Node::NK_AttrName)
    return VLA.toDef(*Parent);

  return nullptr;
}

// Special case, variable in "inherit"
// inherit name
//         ^~~~<---  this is an "AttrName", not variable.
const ExprVar *findInheritVar(const Node &N, const ParentMapAnalysis &PMA,
                              const VariableLookupAnalysis &VLA) {
  if (const Node *Up = PMA.upTo(N, Node::NK_Inherit)) {
    const Node *UpAn = PMA.upTo(N, Node::NK_AttrName);
    if (!UpAn)
      return nullptr;
    const auto &Inh = static_cast<const Inherit &>(*Up);
    const auto &AN = static_cast<const AttrName &>(*UpAn);

    // Skip:
    //
    //    inherit (expr) name1 name2;
    //
    if (Inh.expr())
      return nullptr;

    // Skip dynamic.
    if (!AN.isStatic())
      return nullptr;

    // This attrname will be desugared into an "ExprVar".
    Up = PMA.upTo(Inh, Node::NK_ExprAttrs);
    if (!Up)
      return nullptr;

    const SemaAttrs &SA = static_cast<const ExprAttrs &>(*Up).sema();
    const Node *Var = SA.staticAttrs().at(AN.staticName()).value();
    assert(Var->kind() == Node::NK_ExprVar);
    return static_cast<const ExprVar *>(Var);
  }
  return nullptr;
}

const ExprVar *findVar(const Node &N, const ParentMapAnalysis &PMA,
                       const VariableLookupAnalysis &VLA) {
  if (const ExprVar *InVar = findInheritVar(N, PMA, VLA))
    return InVar;

  return static_cast<const ExprVar *>(PMA.upTo(N, Node::NK_ExprVar));
}

const Definition &findVarDefinition(const ExprVar &Var,
                                    const VariableLookupAnalysis &VLA) {
  LookupResult Result = VLA.query(static_cast<const ExprVar &>(Var));

  if (Result.Kind == ResultKind::Undefined)
    throw UndefinedVarException();

  if (Result.Kind == ResultKind::NoSuchVar)
    throw NoSuchVarException();

  assert(Result.Def);

  return *Result.Def;
}

/// \brief Convert nixf::Definition to lspserver::Location
Location convertToLocation(llvm::StringRef Src, const Definition &Def,
                           URIForFile URI) {
  if (!Def.syntax())
    throw NoLocationForBuiltinVariable();
  assert(Def.syntax());
  return Location{
      .uri = std::move(URI),
      .range = toLSPRange(Src, Def.syntax()->range()),
  };
}

struct NoLocationsFoundInNixpkgsException : std::exception {
  [[nodiscard]] const char *what() const noexcept override {
    return "no locations found in nixpkgs";
  }
};

class WorkerReportedException : std::exception {
  llvm::Error E;

public:
  WorkerReportedException(llvm::Error E) : E(std::move(E)){};

  llvm::Error takeError() { return std::move(E); }
  [[nodiscard]] const char *what() const noexcept override {
    return "worker reported some error";
  }
};

/// \brief Resolve definition by invoking nixpkgs provider.
///
/// Useful for users inspecting nixpkgs packages. For example, someone clicks
/// "with pkgs; [ hello ]", it's better to goto nixpkgs position, instead of
/// "with pkgs;"
class NixpkgsDefinitionProvider {
  AttrSetClient &NixpkgsClient;

  /// \brief Parse nix-rolled location: file:line -> lsp Location
  static Location parseLocation(std::string_view Position) {
    // Firstly, find ":"
    auto Pos = Position.find_first_of(':');
    if (Pos == std::string_view::npos) {
      return Location{
          .uri = URIForFile::canonicalize(Position, Position),
          .range = {{0, 0}, {0, 0}},
      };
    }
    int PosL = std::stoi(std::string(Position.substr(Pos + 1)));
    lspserver::Position P{PosL, 0};
    std::string_view File = Position.substr(0, Pos);
    return Location{
        .uri = URIForFile::canonicalize(File, File),
        .range = {P, P},
    };
  }

public:
  NixpkgsDefinitionProvider(AttrSetClient &NixpkgsClient)
      : NixpkgsClient(NixpkgsClient) {}

  Locations resolveSelector(const nixd::Selector &Sel) {
    std::binary_semaphore Ready(0);
    Expected<AttrPathInfoResponse> Desc = error("not replied");
    auto OnReply = [&Ready, &Desc](llvm::Expected<AttrPathInfoResponse> Resp) {
      if (Resp)
        Desc = *Resp;
      else
        Desc = Resp.takeError();
      Ready.release();
    };
    NixpkgsClient.attrpathInfo(Sel, std::move(OnReply));
    Ready.acquire();

    if (!Desc)
      throw WorkerReportedException(Desc.takeError());

    // Prioritize package location if it exists.
    if (const std::optional<std::string> &Position = Desc->PackageDesc.Position)
      return Locations{parseLocation(*Position)};

    // Use the location in "ValueMeta".
    if (const auto &Loc = Desc->Meta.Location)
      return Locations{*Loc};

    throw NoLocationsFoundInNixpkgsException();
  }
};

/// \brief Try to get "location" by invoking options worker
class OptionsDefinitionProvider {
  AttrSetClient &Client;

public:
  OptionsDefinitionProvider(AttrSetClient &Client) : Client(Client) {}
  void resolveLocations(const std::vector<std::string> &Params,
                        Locations &Locs) {
    std::binary_semaphore Ready(0);
    Expected<OptionInfoResponse> Info = error("not replied");
    OptionCompleteResponse Names;
    auto OnReply = [&Ready, &Info](llvm::Expected<OptionInfoResponse> Resp) {
      Info = std::move(Resp);
      Ready.release();
    };
    // Send request.

    Client.optionInfo(Params, std::move(OnReply));
    Ready.acquire();

    if (!Info) {
      elog("getting locations: {0}", Info.takeError());
      return;
    }

    for (const auto &Decl : Info->Declarations)
      Locs.emplace_back(Decl);
  }
};

/// \brief Get the locations of some attribute path.
///
/// Usually this function will return a list of option declarations via RPC
Locations defineAttrPath(const Node &N, const ParentMapAnalysis &PM,
                         std::mutex &OptionsLock,
                         Controller::OptionMapTy &Options) {
  using PathResult = FindAttrPathResult;
  std::vector<std::string> Scope;
  auto R = findAttrPath(N, PM, Scope);
  Locations Locs;
  if (R == PathResult::OK) {
    std::lock_guard _(OptionsLock);
    // For each option worker, try to get it's decl position.
    for (const auto &[_, Client] : Options) {
      if (AttrSetClient *C = Client->client()) {
        OptionsDefinitionProvider ODP(*C);
        ODP.resolveLocations(Scope, Locs);
      }
    }
  }
  return Locs;
}

/// \brief Get nixpkgs definition from a selector.
Locations defineNixpkgsSelector(const Selector &Sel,
                                AttrSetClient &NixpkgsClient) {
  try {
    // Ask nixpkgs provider information about this selector.
    NixpkgsDefinitionProvider NDP(NixpkgsClient);
    return NDP.resolveSelector(Sel);
  } catch (NoLocationsFoundInNixpkgsException &E) {
    elog("definition/idiom: {0}", E.what());
  } catch (WorkerReportedException &E) {
    elog("definition/idiom/worker: {0}", E.takeError());
  }
  return {};
}

/// \brief Get definiton of select expressions.
Locations defineSelect(const ExprSelect &Sel, const VariableLookupAnalysis &VLA,
                       const ParentMapAnalysis &PM,
                       AttrSetClient &NixpkgsClient) {
  // Currently we can only deal with idioms.
  // Maybe more data-flow analysis will be added though.
  try {
    return defineNixpkgsSelector(mkSelector(Sel, VLA, PM), NixpkgsClient);
  } catch (IdiomSelectorException &E) {
    elog("defintion/idiom/selector: {0}", E.what());
  }
  return {};
}

Locations defineVarStatic(const ExprVar &Var, const VariableLookupAnalysis &VLA,
                          const URIForFile &URI, llvm::StringRef Src) {
  const Definition &Def = findVarDefinition(Var, VLA);
  return {convertToLocation(Src, Def, URI)};
}

template <class T>
std::vector<T> mergeVec(std::vector<T> A, const std::vector<T> &B) {
  A.insert(A.end(), B.begin(), B.end());
  return A;
}

llvm::Expected<Locations>
defineVar(const ExprVar &Var, const VariableLookupAnalysis &VLA,
          const ParentMapAnalysis &PM, AttrSetClient &NixpkgsClient,
          const URIForFile &URI, llvm::StringRef Src) {
  try {
    Locations StaticLocs = defineVarStatic(Var, VLA, URI, Src);

    // Nixpkgs locations.
    try {
      Selector Sel = mkVarSelector(Var, VLA, PM);
      Locations NixpkgsLocs = defineNixpkgsSelector(Sel, NixpkgsClient);
      return mergeVec(std::move(StaticLocs), NixpkgsLocs);
    } catch (std::exception &E) {
      elog("definition/idiom/selector: {0}", E.what());
      return StaticLocs;
    }
  } catch (std::exception &E) {
    elog("definition/static: {0}", E.what());
    return Locations{};
  }
  return error("unreachable code! Please submit an issue");
}

/// \brief Squash a vector into smaller json variant.
template <class T> llvm::json::Value squash(std::vector<T> List) {
  std::size_t Size = List.size();
  switch (Size) {
  case 0:
    return nullptr;
  case 1:
    return std::move(List.back());
  default:
    break;
  }
  return std::move(List);
}

template <class T>
llvm::Expected<llvm::json::Value> squash(llvm::Expected<std::vector<T>> List) {
  if (!List)
    return List.takeError();
  return squash(std::move(*List));
}

} // namespace

const Definition &nixd::findDefinition(const Node &N,
                                       const ParentMapAnalysis &PMA,
                                       const VariableLookupAnalysis &VLA) {
  const ExprVar *Var = findVar(N, PMA, VLA);
  if (!Var) [[unlikely]] {
    if (const Definition *Def = findSelfDefinition(N, PMA, VLA))
      return *Def;
    throw CannotFindVarException();
  }
  assert(Var->kind() == Node::NK_ExprVar);
  return findVarDefinition(*Var, VLA);
}

void Controller::onDefinition(const TextDocumentPositionParams &Params,
                              Callback<llvm::json::Value> Reply) {
  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 Pos = toNixfPosition(Params.position), this]() mutable {
    std::string File(URI.file());
    if (std::shared_ptr<NixTU> TU = getTU(File, Reply)) [[likely]] {
      if (std::shared_ptr<Node> AST = getAST(*TU, Reply)) [[likely]] {
        const VariableLookupAnalysis &VLA = *TU->variableLookup();
        const ParentMapAnalysis &PM = *TU->parentMap();
        const Node *MaybeN = AST->descend({Pos, Pos});
        if (!MaybeN) [[unlikely]] {
          Reply(error("cannot find AST node on given position"));
          return;
        }
        const Node &N = *MaybeN;
        const Node *MaybeUpExpr = PM.upExpr(N);
        if (!MaybeUpExpr) {
          Reply(nullptr);
          return;
        }

        const Node &UpExpr = *MaybeUpExpr;

        return Reply(squash([&]() -> llvm::Expected<Locations> {
          // Special case for inherited names.
          if (const ExprVar *Var = findInheritVar(N, PM, VLA))
            return defineVar(*Var, VLA, PM, *nixpkgsClient(), URI, TU->src());

          switch (UpExpr.kind()) {
          case Node::NK_ExprVar: {
            const auto &Var = static_cast<const ExprVar &>(UpExpr);
            return defineVar(Var, VLA, PM, *nixpkgsClient(), URI, TU->src());
          }
          case Node::NK_ExprSelect: {
            const auto &Sel = static_cast<const ExprSelect &>(UpExpr);
            return defineSelect(Sel, VLA, PM, *nixpkgsClient());
          }
          case Node::NK_ExprAttrs:
            return defineAttrPath(N, PM, OptionsLock, Options);
          default:
            break;
          }
          return error("unknown node type for definition");
        }()));
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
