/// \file
/// \brief Implementation of [Inlay Hints].
/// [Inlay Hints]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_inlayHint
///
/// In nixd, "Inlay Hints" are placed after each "package" node, showing it's
/// version.
///
/// For example
///
///  nixd[: 1.2.3]
///  nix[: 2.19.3]
///
///

#include "AST.h"
#include "CheckReturn.h"
#include "Convert.h"

#include "nixd/CommandLine/Options.h"
#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/CommandLine.h>

#include <semaphore>

using namespace nixd;
using namespace nixf;
using namespace lspserver;
using namespace llvm::cl;

namespace {

opt<bool> EnableInlayHints{"inlay-hints", desc("Enable/Disable inlay-hints"),
                           init(true), cat(NixdCategory)};

/// Ask nixpkgs provider to compute package information, to get inlay-hints.
class NixpkgsInlayHintsProvider {
  AttrSetClient &NixpkgsProvider;
  const VariableLookupAnalysis &VLA;
  const ParentMapAnalysis &PMA;

  /// Only positions contained in this range should be computed && added;
  std::optional<nixf::PositionRange> Range;

  std::vector<InlayHint> &Hints;

  bool rangeOK(const nixf::PositionRange &R) {
    if (!Range)
      return true; // Always OK if there is no limitation.
    return Range->contains(R);
  }

  llvm::StringRef Src;

public:
  NixpkgsInlayHintsProvider(AttrSetClient &NixpkgsProvider,
                            const VariableLookupAnalysis &VLA,
                            const ParentMapAnalysis &PMA,
                            std::optional<lspserver::Range> Range,
                            std::vector<InlayHint> &Hints, llvm::StringRef Src)
      : NixpkgsProvider(NixpkgsProvider), VLA(VLA), PMA(PMA), Hints(Hints),
        Src(Src) {
    if (Range)
      this->Range = toNixfRange(*Range);
  }

  void dfs(const Node *N) {
    if (!N)
      return;
    if (N->kind() == Node::NK_ExprVar) {
      if (havePackageScope(*N, VLA, PMA)) {
        if (!rangeOK(N->positionRange()))
          return;
        // Ask nixpkgs eval to provide it's information.
        // This is relatively slow. Maybe better query a set of packages in the
        // future?
        std::binary_semaphore Ready(0);
        const std::string &Name = static_cast<const ExprVar &>(*N).id().name();
        AttrPathInfoResponse R;
        auto OnReply = [&Ready, &R](llvm::Expected<AttrPathInfoResponse> Resp) {
          if (!Resp) {
            Ready.release();
            return;
          }
          R = *Resp;
          Ready.release();
        };
        NixpkgsProvider.attrpathInfo({Name}, std::move(OnReply));
        Ready.acquire();

        if (const std::optional<std::string> &Version = R.PackageDesc.Version) {
          // Construct inlay hints.
          InlayHint H{
              .position = toLSPPosition(Src, N->rCur()),
              .label = ": " + *Version,
              .kind = InlayHintKind::Designator,
              .range = toLSPRange(Src, N->range()),
          };
          Hints.emplace_back(std::move(H));
        }
      }
    }
    // FIXME: process other node kinds. e.g. ExprSelect.
    for (const Node *Ch : N->children())
      dfs(Ch);
  }
};

} // namespace

void Controller::onInlayHint(const InlayHintsParams &Params,
                             Callback<std::vector<InlayHint>> Reply) {

  using CheckTy = std::vector<InlayHint>;

  // If not enabled, exit early.
  if (!EnableInlayHints)
    return Reply(std::vector<InlayHint>{});

  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 Range = Params.range, this]() mutable {
    const auto File = URI.file();
    return Reply([&]() -> llvm::Expected<CheckTy> {
      const auto TU = CheckDefault(getTU(File));
      const auto AST = CheckDefault(getAST(*TU));
      // Perform inlay hints computation on the range.
      std::vector<InlayHint> Response;
      NixpkgsInlayHintsProvider NP(*nixpkgsClient(), *TU->variableLookup(),
                                   *TU->parentMap(), Range, Response,
                                   TU->src());
      NP.dfs(AST.get());
      return Response;
    }());
  };
  boost::asio::post(Pool, std::move(Action));
}
