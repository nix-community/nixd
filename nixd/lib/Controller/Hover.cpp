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

  /// \brief Make markdown including both package and value description
  static std::string mkMarkdown(const AttrPathInfoResponse &Info) {
    std::ostringstream OS;
    // Package section (if available)
    OS << mkMarkdown(Info.PackageDesc);

    // Value description section
    if (Info.ValueDesc) {
      const auto &VD = *Info.ValueDesc;
      if (!OS.str().empty())
        OS << "\n";
      OS << "## Value" << "\n\n";
      if (!VD.Doc.empty()) {
        OS << VD.Doc << "\n\n";
      }
      OS << "**Arity:** " << VD.Arity << "\n";
      if (!VD.Args.empty()) {
        OS << "**Args:** ";
        for (size_t Idx = 0; Idx < VD.Args.size(); ++Idx) {
          OS << "`" << VD.Args[Idx] << "`";
          if (Idx + 1 < VD.Args.size())
            OS << ", ";
        }
        OS << "\n";
      }
    }

    return OS.str();
  }

public:
  NixpkgsHoverProvider(AttrSetClient &NixpkgsClient)
      : NixpkgsClient(NixpkgsClient) {}

  std::optional<std::string> resolveSelector(const nixd::Selector &Sel) {
    std::binary_semaphore Ready(0);
    std::optional<AttrPathInfoResponse> Info;
    auto OnReply = [&Ready, &Info](llvm::Expected<AttrPathInfoResponse> Resp) {
      if (Resp)
        Info = *Resp;
      else
        elog("nixpkgs provider: {0}", Resp.takeError());
      Ready.release();
    };
    NixpkgsClient.attrpathInfo(Sel, std::move(OnReply));
    Ready.acquire();

    if (!Info)
      return std::nullopt;

    return mkMarkdown(*Info);
  }
};

/// \brief Get nixpkgs hover info from a selector.
std::optional<Hover> hoverNixpkgsSelector(const Selector &Sel,
                                          const nixf::Node &N,
                                          const VariableLookupAnalysis &VLA,
                                          const ParentMapAnalysis &PM,
                                          AttrSetClient &NixpkgsClient,
                                          llvm::StringRef Src) {
  try {
    // Ask nixpkgs provider information about this selector.
    NixpkgsHoverProvider NHP(NixpkgsClient);
    if (std::optional<std::string> Doc = NHP.resolveSelector(Sel)) {
      return Hover{
          .contents =
              MarkupContent{
                  .kind = MarkupKind::Markdown,
                  .value = std::move(*Doc),
              },
          .range = toLSPRange(Src, N.range()),
      };
    }
  } catch (std::exception &E) {
    elog("hover/idiom: {0}", E.what());
  }
  return std::nullopt;
}

/// \brief Get hover info for ExprVar.
std::optional<Hover> hoverVar(const ExprVar &Var,
                              const VariableLookupAnalysis &VLA,
                              const ParentMapAnalysis &PM,
                              AttrSetClient &NixpkgsClient,
                              llvm::StringRef Src) {
  try {
    Selector Sel = idioms::mkVarSelector(Var, VLA, PM);
    return hoverNixpkgsSelector(Sel, Var, VLA, PM, NixpkgsClient, Src);
  } catch (std::exception &E) {
    elog("hover/idiom/selector: {0}", E.what());
  }
  return std::nullopt;
}

/// \brief Get hover info for ExprSelect.
std::optional<Hover> hoverSelect(const ExprSelect &Sel,
                                 const VariableLookupAnalysis &VLA,
                                 const ParentMapAnalysis &PM,
                                 AttrSetClient &NixpkgsClient,
                                 llvm::StringRef Src) {
  try {
    Selector S = idioms::mkSelector(Sel, VLA, PM);
    return hoverNixpkgsSelector(S, Sel, VLA, PM, NixpkgsClient, Src);
  } catch (std::exception &E) {
    elog("hover/idiom/selector: {0}", E.what());
  }
  return std::nullopt;
}

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

      // Try to get hover info from nixpkgs.
      if (auto *Client = nixpkgsClient(); Client) {
        switch (UpExpr.kind()) {
        case Node::NK_ExprVar: {
          const auto &Var = static_cast<const ExprVar &>(UpExpr);
          if (auto H = hoverVar(Var, VLA, PM, *Client, TU->src()))
            return *H;
          break;
        }
        case Node::NK_ExprSelect: {
          const auto &Sel = static_cast<const ExprSelect &>(UpExpr);
          if (auto H = hoverSelect(Sel, VLA, PM, *Client, TU->src()))
            return *H;
          break;
        }
        case Node::NK_ExprAttrs: {
          // Try to get hover info from options.
          auto Scope = std::vector<std::string>();
          const auto R = findAttrPathForOptions(N, PM, Scope);
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
                      .range = toLSPRange(TU->src(), N.range()),
                  };
                }
              }
            }
          }
          break;
        }
        default:
          break;
        }
      }

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
    }());
  };
  boost::asio::post(Pool, std::move(Action));
}
