/// \file
/// \brief Implementation of [Go to Definition]
/// [Go to Definition]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_definition

#include "Definition.h"
#include "AST.h"
#include "Convert.h"

#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>

#include <llvm/Support/Error.h>

#include <nixf/Basic/Nodes/Attrs.h>
#include <nixf/Basic/Nodes/Basic.h>
#include <nixf/Sema/VariableLookup.h>

#include <semaphore>

using namespace nixd;
using namespace nixf;
using namespace lspserver;
using namespace llvm;

using LookupResult = VariableLookupAnalysis::LookupResult;
using ResultKind = VariableLookupAnalysis::LookupResultKind;

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

Expected<Location> staticDef(URIForFile URI, const Node &N,
                             const ParentMapAnalysis &PMA,
                             const VariableLookupAnalysis &VLA) {
  Expected<const Definition &> ExpDef = findDefinition(N, PMA, VLA);
  if (!ExpDef)
    return ExpDef.takeError();

  if (ExpDef->isBuiltin())
    return error("this is a builtin variable defined by nix interpreter");
  assert(ExpDef->syntax());

  return Location{
      .uri = std::move(URI),
      .range = toLSPRange(ExpDef->syntax()->range()),
  };
}

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
    lspserver::Position P{PosL, PosL};
    std::string_view File = Position.substr(0, Pos);
    return Location{
        .uri = URIForFile::canonicalize(File, File),
        .range = {P, P},
    };
  }

public:
  NixpkgsDefinitionProvider(AttrSetClient &NixpkgsClient)
      : NixpkgsClient(NixpkgsClient) {}

  Expected<lspserver::Location> resolvePackage(std::vector<std::string> Scope,
                                               std::string Name) {
    std::binary_semaphore Ready(0);
    Expected<PackageDescription> Desc = error("not replied");
    auto OnReply = [&Ready, &Desc](llvm::Expected<AttrPathInfoResponse> Resp) {
      if (Resp)
        Desc = *Resp;
      else
        Desc = Resp.takeError();
      Ready.release();
    };
    Scope.emplace_back(std::move(Name));
    NixpkgsClient.attrpathInfo(Scope, std::move(OnReply));
    Ready.acquire();

    if (!Desc)
      return Desc.takeError();

    if (!Desc->Position)
      return error("meta.position is not available for this package");

    try {
      return parseLocation(*Desc->Position);
    } catch (std::exception &E) {
      return error(E.what());
    }
  }
};

} // namespace

Expected<const Definition &>
nixd::findDefinition(const Node &N, const ParentMapAnalysis &PMA,
                     const VariableLookupAnalysis &VLA) {
  const ExprVar *Var = findVar(N, PMA, VLA);
  if (!Var) [[unlikely]] {
    if (const Definition *Def = findSelfDefinition(N, PMA, VLA))
      return *Def;
    return error("cannot find variable on given position");
  }
  assert(Var->kind() == Node::NK_ExprVar);
  LookupResult Result = VLA.query(static_cast<const ExprVar &>(*Var));

  if (Result.Kind == ResultKind::Undefined)
    return error("this varaible is undefined");

  if (Result.Kind == ResultKind::NoSuchVar)
    return error("this varaible is not used in var lookup (duplicated attr?)");

  assert(Result.Def);

  return *Result.Def;
}

void Controller::onDefinition(const TextDocumentPositionParams &Params,
                              Callback<Location> Reply) {
  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 Pos = toNixfPosition(Params.position), this]() mutable {
    std::string File(URI.file());
    if (std::shared_ptr<NixTU> TU = getTU(File, Reply)) [[likely]] {
      if (std::shared_ptr<Node> AST = getAST(*TU, Reply)) [[likely]] {
        const VariableLookupAnalysis &VLA = *TU->variableLookup();
        const ParentMapAnalysis &PM = *TU->parentMap();
        const Node *N = AST->descend({Pos, Pos});
        if (!N) [[unlikely]] {
          Reply(error("cannot find AST node on given position"));
          return;
        }
        if (havePackageScope(*N, VLA, PM) && nixpkgsClient()) {
          // Ask nixpkgs client what's current package documentation.
          NixpkgsDefinitionProvider NDP(*nixpkgsClient());
          auto [Scope, Name] = getScopeAndPrefix(*N, PM);
          Expected<Location> Loc =
              NDP.resolvePackage(std::move(Scope), std::move(Name));
          if (Loc) {
            Reply(std::move(*Loc));
            return;
          }
          elog("cannot get nixpkgs definition for this package: {0}",
               Loc.takeError());
        }
        Reply(staticDef(URI, *N, PM, VLA));
        return;
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
