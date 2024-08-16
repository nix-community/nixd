/// \file
/// \brief Implementation of [Document Link].
/// [Document Link]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_documentLink
///
/// In Nix language, there are a few interesting constructs worth highlighting:
///
///   1. URL literal. Writing URls directly without quotes, supported by vscode.
///   2. Path. (Append "default.nix", perhaps)
///   3. Search Path <>
///     FIXME: support search path.
///   4. Home Path: (begins with ~)
///     FIXME: support home path.
///   5. Flake Reference
///     FIXME: support flake ref
///

#include "Convert.h"

#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>

#include <filesystem>

using namespace nixd;
using namespace lspserver;
using namespace nixf;

namespace {

/// \brief Resolve expr path to "real" path.
std::optional<std::string> resolveExprPath(const std::string &BasePath,
                                           const std::string &ExprPath) {

  namespace fs = std::filesystem;
  // FIXME: we do not use complete "symbolic resolve" here.
  fs::path Path = fs::absolute((BasePath)).remove_filename().append(ExprPath);

  if (!fs::exists(Path))
    return std::nullopt;

  if (fs::is_directory(Path))
    return Path.append("default.nix");

  return Path;
}

void dfs(const Node *N, const std::string &BasePath,
         std::vector<DocumentLink> &Links, llvm::StringRef Src) {
  if (!N)
    return;

  switch (N->kind()) {
  case Node::NK_ExprPath: {
    // Resolve the path.
    const auto &Path = static_cast<const ExprPath &>(*N);
    if (Path.parts().isLiteral()) {
      // Provide literal path linking.
      if (auto Link = resolveExprPath(BasePath, Path.parts().literal())) {
        Links.emplace_back(
            DocumentLink{.range = toLSPRange(Src, N->range()),
                         .target = URIForFile::canonicalize(*Link, *Link)});
      }
    }
    return;
  }
  default:
    break;
  }

  // Traverse on all children
  for (const Node *Ch : N->children()) {
    dfs(Ch, BasePath, Links, Src);
  }
}

} // namespace

void Controller::onDocumentLink(
    const DocumentLinkParams &Params,
    lspserver::Callback<std::vector<DocumentLink>> Reply) {
  auto Action = [File = Params.textDocument.uri.file().str(),
                 Reply = std::move(Reply), this]() mutable {
    if (std::shared_ptr<NixTU> TU = getTU(File, Reply)) [[likely]] {
      if (std::shared_ptr<nixf::Node> AST = getAST(*TU, Reply)) [[likely]] {
        // Traverse the AST, provide the links
        std::vector<DocumentLink> Links;
        dfs(AST.get(), File, Links, TU->src());
        Reply(std::move(Links));
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
