/// \file
/// \brief Implementation of [Document Link].
/// [Document Link]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_documentLink
///
/// In nix language there are a few things interesting and it's nice to
/// underline them.
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
using std::string;

// Resolve expr path to "real" path.
std::optional<std::string> resolveExprPath(const std::string &File) {
  // FIXME: we do not use complete "symbolic resolve" here.
  auto Path = std::filesystem::absolute((File));

  if (!std::filesystem::exists(Path))
    return std::nullopt;

  if (std::filesystem::is_directory(Path))
    return Path.append("default.nix");

  return std::filesystem::absolute(File);
}

void dfs(const Node *N, std::vector<DocumentLink> &Links) {
  if (!N)
    return;

  switch (N->kind()) {
  case Node::NK_ExprPath: {
    // Resolve the path.
    const auto &Path = static_cast<const ExprPath &>(*N);
    if (Path.parts().isLiteral()) {
      // Provide literal path linking.
      if (auto Link = resolveExprPath(Path.parts().literal())) {
        Links.emplace_back(
            DocumentLink{.range = toLSPRange(N->range()),
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
    dfs(Ch, Links);
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
        dfs(AST.get(), Links);
        Reply(std::move(Links));
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
