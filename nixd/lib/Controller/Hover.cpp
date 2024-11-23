/// \file
/// \brief Implementation of [Hover Request].
/// [Hover Request]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_hover

#include "AST.h"
#include "CheckReturn.h"
#include "Convert.h"

#include "nixd/Controller/Controller.h"
#include "nixd/Protocol/AttrSet.h"

#include <boost/asio/post.hpp>

#include <llvm/Support/Error.h>

#include <semaphore>
#include <sstream>

using namespace nixd;
using namespace llvm::json;
using namespace nixf;
using namespace lspserver;

namespace {

class OptionsHoverProvider {
  AttrSetClient &Client;

public:
  OptionsHoverProvider(AttrSetClient &Client) : Client(Client) {}
  std::optional<OptionDescription>
  resolveHover(const std::vector<std::string> &Scope) {
    std::binary_semaphore Ready(0);
    std::optional<OptionDescription> Desc;
    auto OnReply = [&Ready, &Desc](llvm::Expected<OptionInfoResponse> Resp) {
      if (Resp)
        Desc = *Resp;
      else
        elog("options hover: {0}", Resp.takeError());
      Ready.release();
    };

    Client.optionInfo(Scope, std::move(OnReply));
    Ready.acquire();

    return Desc;
  }
};

/// \brief Provide package information, library information ... , from nixpkgs.
class NixpkgsHoverProvider {
  AttrSetClient &NixpkgsClient;

  /// \brief Make markdown documentation by package description
  ///
  /// FIXME: there are many markdown generation in language server.
  /// Maybe we can add structured generating first?
  static std::string mkMarkdown(const PackageDescription &Package) {
    std::ostringstream OS;
    // Make each field a new section

    if (Package.Name) {
      OS << "`" << *Package.Name << "`";
      OS << "\n";
    }

    // Make links to homepage.
    if (Package.Homepage) {
      OS << "[homepage](" << *Package.Homepage << ")";
      OS << "\n";
    }

    if (Package.Description) {
      OS << "## Description"
         << "\n\n";
      OS << *Package.Description;
      OS << "\n\n";

      if (Package.LongDescription) {
        OS << "\n\n";
        OS << *Package.LongDescription;
        OS << "\n\n";
      }
    }

    return OS.str();
  }

public:
  NixpkgsHoverProvider(AttrSetClient &NixpkgsClient)
      : NixpkgsClient(NixpkgsClient) {}

  std::optional<std::string> resolvePackage(const Selector &Sel) {
    std::binary_semaphore Ready(0);
    std::optional<AttrPathInfoResponse> Desc;
    auto OnReply = [&Ready, &Desc](llvm::Expected<AttrPathInfoResponse> Resp) {
      if (Resp)
        Desc = *Resp;
      else
        elog("nixpkgs provider: {0}", Resp.takeError());
      Ready.release();
    };
    NixpkgsClient.attrpathInfo(Sel, std::move(OnReply));
    Ready.acquire();

    if (!Desc)
      return std::nullopt;

    return mkMarkdown(Desc->PackageDesc);
  }
};

class HoverProvider {
  const NixTU &TU;
  const VariableLookupAnalysis &VLA;
  const ParentMapAnalysis &PM;

  [[nodiscard]] std::optional<Hover>
  mkHover(std::optional<std::string> Doc, nixf::LexerCursorRange Range) const {
    if (!Doc)
      return std::nullopt;
    return Hover{
        .contents =
            MarkupContent{
                .kind = MarkupKind::Markdown,
                .value = std::move(*Doc),
            },
        .range = toLSPRange(TU.src(), Range),
    };
  }

public:
  HoverProvider(const NixTU &TU, const VariableLookupAnalysis &VLA,
                const ParentMapAnalysis &PM)
      : TU(TU), VLA(VLA), PM(PM) {}

  std::optional<Hover> hoverVar(const nixf::Node &N,
                                AttrSetClient &Client) const {
    if (havePackageScope(N, VLA, PM)) {
      // Ask nixpkgs client what's current package documentation.
      auto NHP = NixpkgsHoverProvider(Client);
      auto [Scope, Name] = getScopeAndPrefix(N, PM);
      Scope.emplace_back(Name);
      return mkHover(NHP.resolvePackage(Scope), N.range());
    }

    return std::nullopt;
  }

  std::optional<Hover> hoverSelect(const nixf::ExprSelect &Select,
                                   AttrSetClient &Client) const {
    // The base expr for selecting.
    const nixf::Expr &BaseExpr = Select.expr();

    if (BaseExpr.kind() != Node::NK_ExprVar) {
      return std::nullopt;
    }

    const auto &Var = static_cast<const nixf::ExprVar &>(BaseExpr);
    try {
      Selector Sel =
          idioms::mkSelector(Select, idioms::mkVarSelector(Var, VLA, PM));
      auto NHP = NixpkgsHoverProvider(Client);
      return mkHover(NHP.resolvePackage(Sel), Select.range());
    } catch (std::exception &E) {
      log("hover/select skipped, reason: {0}", E.what());
    }
    return std::nullopt;
  }

  std::optional<Hover>
  hoverAttrPath(const nixf::Node &N, std::mutex &OptionsLock,
                const Controller::OptionMapTy &Options) const {
    auto Scope = std::vector<std::string>();
    const auto R = findAttrPath(N, PM, Scope);
    if (R == FindAttrPathResult::OK) {
      std::lock_guard _(OptionsLock);
      for (const auto &[_, Client] : Options) {
        if (AttrSetClient *C = Client->client()) {
          OptionsHoverProvider OHP(*C);
          std::optional<OptionDescription> Desc = OHP.resolveHover(Scope);
          std::string Docs;
          if (Desc) {
            if (Desc->Type) {
              std::string TypeName = Desc->Type->Name.value_or("");
              std::string TypeDesc = Desc->Type->Description.value_or("");
              Docs += llvm::formatv("{0} ({1})", TypeName, TypeDesc);
            } else {
              Docs += "? (missing type)";
            }
            if (Desc->Description) {
              Docs += "\n\n" + Desc->Description.value_or("");
            }
            return Hover{
                .contents =
                    MarkupContent{
                        .kind = MarkupKind::Markdown,
                        .value = std::move(Docs),
                    },
                .range = toLSPRange(TU.src(), N.range()),
            };
          }
        }
      }
    }
    return std::nullopt;
  }
};

} // namespace

void Controller::onHover(const TextDocumentPositionParams &Params,
                         Callback<std::optional<Hover>> Reply) {
  using CheckTy = std::optional<Hover>;
  auto Action = [Reply = std::move(Reply),
                 File = std::string(Params.textDocument.uri.file()),
                 RawPos = Params.position, this]() mutable {
    return Reply([&]() -> llvm::Expected<CheckTy> {
      const auto TU = CheckDefault(getTU(File));
      const auto AST = CheckDefault(getAST(*TU));
      const auto Pos = nixf::Position{RawPos.line, RawPos.character};
      const auto &N = *CheckDefault(AST->descend({Pos, Pos}));

      const auto Name = std::string(N.name());
      const auto &VLA = *TU->variableLookup();
      const auto &PM = *TU->parentMap();
      const auto &UpExpr = *CheckDefault(PM.upExpr(N));

      const auto Provider = HoverProvider(*TU, VLA, PM);

      const auto HoverByCase = [&]() -> std::optional<Hover> {
        switch (UpExpr.kind()) {
        case Node::NK_ExprVar:
          return Provider.hoverVar(N, *nixpkgsClient());
        case Node::NK_ExprSelect:
          return Provider.hoverSelect(
              static_cast<const nixf::ExprSelect &>(UpExpr), *nixpkgsClient());
        case Node::NK_ExprAttrs:
          return Provider.hoverAttrPath(N, OptionsLock, Options);
        default:
          return std::nullopt;
        }
      }();

      if (HoverByCase) {
        return HoverByCase.value();
      } else {
        // Reply it's kind by static analysis
        // FIXME: support more.
        return Hover{
            .contents =
                MarkupContent{
                    .kind = MarkupKind::Markdown,
                    .value = "`" + Name + "`",
                },
            .range = toLSPRange(TU->src(), N.range()),
        };
      }
    }());
  };
  boost::asio::post(Pool, std::move(Action));
}
