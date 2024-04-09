/// \file
/// \brief Implementation of [Semantic Tokens].
/// [Semantic Tokens]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_semanticTokens

#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>
#include <lspserver/Protocol.h>
#include <nixf/Basic/Nodes/Attrs.h>
#include <nixf/Basic/Nodes/Expr.h>
#include <nixf/Basic/Nodes/Lambda.h>
#include <nixf/Basic/Range.h>
#include <nixf/Sema/VariableLookup.h>

using namespace nixd;
using namespace lspserver;
using namespace nixf;

namespace {

enum SemaType {
  ST_Function,
  ST_String,
  ST_Number,
  ST_Select,
  ST_Builtin,
  ST_Defined,
  ST_FromWith,
  ST_Undefined,
  ST_Null,
  ST_Bool,
  ST_AttrName,
  ST_LambdaArg,
  ST_LambdaFormal,
};

enum SemaModifiers {
  SM_Builtin = 1 << 0,
  SM_Deprecated = 1 << 1,
  SM_Dynamic = 1 << 2,
};

struct RawSemanticToken {
  lspserver::Position Pos;
  bool operator<(const RawSemanticToken &Other) const {
    return Pos < Other.Pos;
  }
  unsigned Length;
  unsigned TokenType;
  unsigned TokenModifiers;
};

class SemanticTokenBuilder {

  const VariableLookupAnalysis &VLA;

  nixf::Position Previous = {0, 0};

  std::vector<RawSemanticToken> Raw;

public:
  SemanticTokenBuilder(const VariableLookupAnalysis &VLA) : VLA(VLA) {}
  void addImpl(nixf::LexerCursor Pos, unsigned Length, unsigned TokenType,
               unsigned TokenModifiers) {
    Raw.emplace_back(RawSemanticToken{
        {static_cast<int>(Pos.line()), static_cast<int>(Pos.column())},
        Length,
        TokenType,
        TokenModifiers});
  }

  void add(const Node &N, unsigned TokenType, unsigned TokenModifiers) {
    if (skip(N))
      return;
    addImpl(N.lCur(), len(N), TokenType, TokenModifiers);
  }

  static bool skip(const Node &N) {
    // Skip cross-line strings.
    return N.range().lCur().line() != N.range().rCur().line();
  }

  static unsigned len(const Node &N) {
    return N.range().rCur().offset() - N.range().lCur().offset();
  }

  void dfs(const ExprString &Str) {
    unsigned Modifers = 0;
    if (!Str.isLiteral())
      return;
    add(Str, ST_String, Modifers);
  }

  void dfs(const ExprVar &Var) {
    if (Var.id().name() == "true" || Var.id().name() == "false") {
      add(Var, ST_Bool, SM_Builtin);
      return;
    }

    if (Var.id().name() == "null") {
      add(Var, ST_Null, 0);
      return;
    }

    auto Result = VLA.query(Var);
    using ResultKind = VariableLookupAnalysis::LookupResultKind;
    if (Result.Def && Result.Def->isBuiltin()) {
      add(Var, ST_Builtin, SM_Builtin);
      return;
    }
    if (Result.Kind == ResultKind::Defined) {
      add(Var, ST_Defined, 0);
      return;
    }
    if (Result.Kind == ResultKind::FromWith) {
      add(Var, ST_FromWith, SM_Dynamic);
      return;
    }

    add(Var, ST_Defined, SM_Deprecated);
  }

  void dfs(const ExprSelect &Select) {
    dfs(&Select.expr());
    dfs(Select.defaultExpr());
    if (!Select.path())
      return;
    for (const std::shared_ptr<nixf::AttrName> &Name : Select.path()->names()) {
      if (!Name)
        continue;
      const AttrName &AN = *Name;
      if (AN.isStatic()) {
        if (AN.kind() == AttrName::ANK_ID) {
          add(AN, ST_Select, 0);
        }
      }
    }
  }

  void dfs(const SemaAttrs &SA) {
    for (const auto &[Name, Attr] : SA.staticAttrs()) {
      if (!Attr.value())
        continue;
      add(Attr.key(), ST_AttrName, 0);
      dfs(Attr.value());
    }
    for (const auto &Attr : SA.dynamicAttrs()) {
      dfs(Attr.value());
    }
  }

  void dfs(const LambdaArg &Arg) {
    if (Arg.id())
      add(*Arg.id(), ST_LambdaArg, 0);
    // Color deduplicated formals.
    if (Arg.formals())
      for (const auto &[_, Formal] : Arg.formals()->dedup()) {
        if (Formal->id()) {
          add(*Formal->id(), ST_LambdaFormal, 0);
        }
      }
  }

  void dfs(const ExprLambda &Lambda) {
    if (Lambda.arg()) {
      dfs(*Lambda.arg());
    }
    dfs(Lambda.body());
  }

  void dfs(const Node *AST) {
    if (!AST)
      return;
    switch (AST->kind()) {
    case Node::NK_ExprLambda: {
      const auto &Lambda = static_cast<const ExprLambda &>(*AST);
      dfs(Lambda);
      break;
    }
    case Node::NK_ExprString: {
      const auto &Str = static_cast<const ExprString &>(*AST);
      dfs(Str);
      break;
    }
    case Node::NK_ExprVar: {
      const auto &Var = static_cast<const ExprVar &>(*AST);
      dfs(Var);
      break;
    }
    case Node::NK_ExprSelect: {
      const auto &Select = static_cast<const ExprSelect &>(*AST);
      dfs(Select);
      break;
    }
    case Node::NK_ExprAttrs: {
      const SemaAttrs &SA = static_cast<const ExprAttrs &>(*AST).sema();
      dfs(SA);
      break;
    }
    default:
      for (const Node *Ch : AST->children()) {
        dfs(Ch);
      }
    }
  };

  std::vector<SemanticToken> finish() {
    std::vector<SemanticToken> Tokens;
    std::sort(Raw.begin(), Raw.end());
    lspserver::Position Prev{0, 0};
    for (auto Elm : Raw) {
      assert(Elm.Pos.line - Prev.line >= 0);
      unsigned DeltaLine = Elm.Pos.line - Prev.line;
      unsigned DeltaCol =
          DeltaLine ? Elm.Pos.character : Elm.Pos.character - Prev.character;
      Prev = Elm.Pos;
      Tokens.emplace_back(DeltaLine, DeltaCol, Elm.Length, Elm.TokenType,
                          Elm.TokenModifiers);
    }
    return Tokens;
  }
};

} // namespace

void Controller::onSemanticTokens(const SemanticTokensParams &Params,
                                  Callback<SemanticTokens> Reply) {
  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 this]() mutable {
    if (std::shared_ptr<NixTU> TU =
            getTU(URI.file().str(), Reply, /*Ignore=*/true)) {
      if (std::shared_ptr<Node> AST = getAST(*TU, Reply)) {
        SemanticTokenBuilder Builder(*TU->variableLookup());
        Builder.dfs(AST.get());
        Reply(SemanticTokens{.tokens = Builder.finish()});
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
