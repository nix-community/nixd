/// \file
/// \brief Implementation of [Code Completion].
/// [Code Completion]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_completion

#include "Convert.h"

#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>

using namespace nixd;
using namespace lspserver;
using namespace nixf;

namespace {

/// Set max completion size to this value, we don't want to send large lists
/// because of slow IO.
/// Items exceed this size should be marked "incomplete" and recomputed.
constexpr int MaxCompletionSize = 30;

struct ExceedSizeError : std::exception {
  [[nodiscard]] const char *what() const noexcept override {
    return "Size exceeded";
  }
};

void addItem(std::vector<CompletionItem> &Items, CompletionItem Item) {
  if (Items.size() >= MaxCompletionSize) {
    throw ExceedSizeError();
  }
  Items.emplace_back(std::move(Item));
}

class VLACompletionProvider {
  const VariableLookupAnalysis &VLA;

  static CompletionItemKind getCompletionItemKind(const Definition &Def) {
    if (Def.isBuiltin()) {
      return CompletionItemKind::Keyword;
    }
    return CompletionItemKind::Variable;
  }

  /// Collect definition on some env, and also it's ancestors.
  void collectDef(std::vector<CompletionItem> &Items, const EnvNode *Env,
                  const std::string &Prefix) {
    if (!Env)
      return;
    collectDef(Items, Env->parent(), Prefix);
    for (const auto &[Name, Def] : Env->defs()) {
      if (Name.starts_with(
              "__")) // These names are nix internal implementation, skip.
        continue;
      assert(Def);
      if (Name.starts_with(Prefix)) {
        addItem(Items, CompletionItem{
                           .label = Name,
                           .kind = getCompletionItemKind(*Def),
                       });
      }
    }
  }

public:
  VLACompletionProvider(const VariableLookupAnalysis &VLA) : VLA(VLA) {}

  /// Perform code completion right after this node.
  /// \returns true if the completion list is in-complete
  bool complete(const nixf::Node &Desc, std::vector<CompletionItem> &Items,
                const ParentMapAnalysis &PM) {
    std::string Prefix; // empty string, accept all prefixes
    if (Desc.kind() == Node::NK_Identifer)
      Prefix = static_cast<const Identifier &>(Desc).name();
    try {
      const nixf::Node *N = &Desc; // @nonnull
      while (!VLA.env(N) && PM.query(*N) && !PM.isRoot(*N))
        N = PM.query(*N);

      assert(N);
      collectDef(Items, VLA.env(N), Prefix);
    } catch (const ExceedSizeError &Err) {
      return true;
    }
    return false;
  }
};

} // namespace

void Controller::onCompletion(const CompletionParams &Params,
                              Callback<CompletionList> Reply) {
  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 Pos = toNixfPosition(Params.position), this]() mutable {
    std::string File(URI.file());
    if (std::shared_ptr<NixTU> TU = getTU(File, Reply)) [[likely]] {
      if (std::shared_ptr<nixf::Node> AST = getAST(*TU, Reply)) [[likely]] {
        const nixf::Node *Desc = AST->descend({Pos, Pos});
        if (!Desc) {
          Reply(error("cannot find corresponding node on given position"));
          return;
        }
        CompletionList List;
        VLACompletionProvider VLAP(*TU->variableLookup());
        List.isIncomplete = VLAP.complete(*Desc, List.items, *TU->parentMap());
        Reply(std::move(List));
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
