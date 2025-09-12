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
    using VariableLookupAnalysis::LookupResultKind::Defined;
    using Params = AttrPathInfoParams;
    if (!N)
      return;

    // Build the attribute path info selector.
    const auto Selector = [&]() -> Params {
      if (!havePackageScope(*N, VLA, PMA) || !rangeOK(N->positionRange()))
        return {};
      switch (N->kind()) {
      default:
        return {};
      case Node::NK_ExprVar: {
        const auto &Var = static_cast<const ExprVar &>(*N);
        const std::string &Name = Var.id().name();
        if (Name == nixd::idioms::Lib) // Skip "lib"
          return {};
        const auto &Lookup = VLA.query(Var);
        return (Lookup.Kind != Defined) ? Params{Name} : Params{};
      }
      case Node::NK_ExprSelect: {
        const auto &Sel = static_cast<const ExprSelect &>(*N);
        Params P;
        P.emplace_back(Sel.expr().src(Src));
        for (const auto &Name : Sel.path()->names())
          P.emplace_back(Name->src(Src));
        return P;
      }
      }
    }();

    // Ask nixpkgs eval to provide it's information.
    // This is relatively slow. Maybe better query a set of packages in the
    // future?
    if (!Selector.empty()) {
      std::binary_semaphore Ready(0);
      AttrPathInfoResponse R;
      auto OnReply = [&Ready, &R](llvm::Expected<AttrPathInfoResponse> Resp) {
        if (!Resp) {
          Ready.release();
          return;
        }
        R = *Resp;
        Ready.release();
      };
      NixpkgsProvider.attrpathInfo(Selector, std::move(OnReply));
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
