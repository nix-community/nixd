/// \file
/// \brief Implementation of [Hover Request].
/// [Hover Request]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_hover

#include "AST.h"
#include "Convert.h"

#include "nixd/Controller/Controller.h"

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
      OS << "## Description" << "\n\n";
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

  std::optional<std::string> resolvePackage(std::vector<std::string> Scope,
                                            std::string Name) {
    std::binary_semaphore Ready(0);
    std::optional<PackageDescription> Desc;
    auto OnReply = [&Ready, &Desc](llvm::Expected<AttrPathInfoResponse> Resp) {
      if (Resp)
        Desc = *Resp;
      else
        elog("nixpkgs provider: {0}", Resp.takeError());
      Ready.release();
    };
    Scope.emplace_back(std::move(Name));
    NixpkgsClient.attrpathInfo(Scope, std::move(OnReply));
    Ready.acquire();

    if (!Desc)
      return std::nullopt;

    return mkMarkdown(*Desc);
  }
};

} // namespace

void Controller::onHover(const TextDocumentPositionParams &Params,
                         Callback<std::optional<Hover>> Reply) {
  auto Action = [Reply = std::move(Reply),
                 File = std::string(Params.textDocument.uri.file()),
                 RawPos = Params.position, this]() mutable {
    if (std::shared_ptr<NixTU> TU = getTU(File, Reply)) [[likely]] {
      if (std::shared_ptr<nixf::Node> AST = getAST(*TU, Reply)) [[likely]] {
        nixf::Position Pos{RawPos.line, RawPos.character};
        const nixf::Node *N = AST->descend({Pos, Pos});
        if (!N) {
          Reply(std::nullopt);
          return;
        }
        std::string Name = N->name();
        const VariableLookupAnalysis &VLA = *TU->variableLookup();
        const ParentMapAnalysis &PM = *TU->parentMap();
        if (havePackageScope(*N, VLA, PM) && nixpkgsClient()) {
          // Ask nixpkgs client what's current package documentation.
          NixpkgsHoverProvider NHP(*nixpkgsClient());
          auto [Scope, Name] = getScopeAndPrefix(*N, PM);
          if (std::optional<std::string> Doc =
                  NHP.resolvePackage(Scope, Name)) {
            Reply(Hover{
                .contents =
                    MarkupContent{
                        .kind = MarkupKind::Markdown,
                        .value = std::move(*Doc),
                    },
                .range = toLSPRange(N->range()),
            });
            return;
          }
        }

        std::vector<std::string> Scope;
        auto R = findAttrPath(*N, PM, Scope);
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
                Reply(Hover{
                    .contents =
                        MarkupContent{
                            .kind = MarkupKind::Markdown,
                            .value = std::move(Docs),
                        },
                    .range = toLSPRange(N->range()),
                });
                return;
              }
            }
          }
        }

        // Reply it's kind by static analysis
        // FIXME: support more.
        Reply(Hover{
            .contents =
                MarkupContent{
                    .kind = MarkupKind::Markdown,
                    .value = "`" + Name + "`",
                },
            .range = toLSPRange(N->range()),
        });
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
