/// \file
/// \brief Implementation of [Document Symbol].
/// [Document Symbol]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_documentSymbol

#include "lib/Controller/Convert.h"
#include "lspserver/Protocol.h"
#include "nixd/Controller/Controller.h"
#include "nixf/Basic/Nodes/Attrs.h"
#include "nixf/Basic/Nodes/Lambda.h"
#include "nixf/Sema/VariableLookup.h"

#include <boost/asio/post.hpp>
#include <string>

using namespace nixd;
using namespace lspserver;
using namespace nixf;

namespace {

std::string getLambdaName(const ExprLambda &Lambda) {
  if (!Lambda.arg() || !Lambda.arg()->id())
    return "(anonymous lambda)";
  return Lambda.arg()->id()->name();
}

lspserver::Range getLambdaSelectionRage(const ExprLambda &Lambda) {
  if (!Lambda.arg()) {
    return toLSPRange(Lambda.range());
  }

  if (!Lambda.arg()->id()) {
    assert(Lambda.arg()->formals());
    return toLSPRange(Lambda.arg()->formals()->range());
  }
  return toLSPRange(Lambda.arg()->id()->range());
}

lspserver::Range getAttrRange(const Attribute &Attr) {
  auto LCur = toLSPPosition(Attr.key().lCur());
  if (Attr.value())
    return {LCur, toLSPPosition(Attr.value()->rCur())};
  return {LCur, toLSPPosition(Attr.key().rCur())};
}

/// Make variable's entry rich.
void richVar(const ExprVar &Var, DocumentSymbol &Sym,
             const VariableLookupAnalysis &VLA) {
  if (Var.id().name() == "true" || Var.id().name() == "false") {
    Sym.kind = SymbolKind::Boolean;
    Sym.detail = "builtin boolean";
    return;
  }

  if (Var.id().name() == "null") {
    Sym.kind = SymbolKind::Null;
    Sym.detail = "null";
    return;
  }

  auto Result = VLA.query(Var);
  using ResultKind = VariableLookupAnalysis::LookupResultKind;
  if (Result.Kind == ResultKind::Defined)
    Sym.kind = SymbolKind::Constant;
  else if (Result.Kind == ResultKind::FromWith)
    Sym.kind = SymbolKind::Variable;
  else {
    Sym.deprecated = true;
    return;
  }

  if (Result.Def->isBuiltin())
    Sym.kind = SymbolKind::Event;
}

/// Collect document symbol on AST.
void collect(const Node *AST, std::vector<DocumentSymbol> &Symbols,
             const VariableLookupAnalysis &VLA) {
  if (!AST)
    return;
  switch (AST->kind()) {

  case Node::NK_ExprString: {
    const auto &Str = static_cast<const ExprString &>(*AST);
    DocumentSymbol Sym{
        .name = Str.isLiteral() ? Str.literal() : "(dynamic string)",
        .detail = "string",
        .kind = SymbolKind::String,
        .deprecated = false,
        .range = toLSPRange(Str.range()),
        .selectionRange = toLSPRange(Str.range()),
        .children = {},
    };
    Symbols.emplace_back(std::move(Sym));
    break;
  }
  case Node::NK_ExprInt: {
    const auto &Int = static_cast<const ExprInt &>(*AST);
    DocumentSymbol Sym{
        .name = std::to_string(Int.value()),
        .detail = "integer",
        .kind = SymbolKind::Number,
        .deprecated = false,
        .range = toLSPRange(Int.range()),
        .selectionRange = toLSPRange(Int.range()),
        .children = {},
    };
    Symbols.emplace_back(std::move(Sym));
    break;
  }
  case Node::NK_ExprFloat: {
    const auto &Float = static_cast<const ExprFloat &>(*AST);
    DocumentSymbol Sym{
        .name = std::to_string(Float.value()),
        .detail = "float",
        .kind = SymbolKind::Number,
        .deprecated = false,
        .range = toLSPRange(Float.range()),
        .selectionRange = toLSPRange(Float.range()),
        .children = {},
    };
    Symbols.emplace_back(std::move(Sym));
    break;
  }
  case Node::NK_AttrName: {
    const auto &AN = static_cast<const AttrName &>(*AST);
    DocumentSymbol Sym{
        .name = AN.isStatic() ? AN.staticName() : "(dynamic attribute name)",
        .detail = "attribute name",
        .kind = SymbolKind::Property,
        .deprecated = false,
        .range = toLSPRange(AN.range()),
        .selectionRange = toLSPRange(AN.range()),
        .children = {},
    };
    Symbols.emplace_back(std::move(Sym));
    break;
  }
  case Node::NK_ExprVar: {
    const auto &Var = static_cast<const ExprVar &>(*AST);
    DocumentSymbol Sym{
        .name = Var.id().name(),
        .detail = "identifier",
        .kind = SymbolKind::Variable,
        .deprecated = false,
        .range = toLSPRange(Var.range()),
        .selectionRange = toLSPRange(Var.range()),
        .children = {},
    };
    richVar(Var, Sym, VLA);
    Symbols.emplace_back(std::move(Sym));
    break;
  }
  case Node::NK_ExprLambda: {
    // Lambda, special formals.
    std::vector<DocumentSymbol> Children;
    const auto &Lambda = static_cast<const ExprLambda &>(*AST);
    collect(Lambda.body(), Children, VLA);
    DocumentSymbol Sym{
        .name = getLambdaName(Lambda),
        .detail = "lambda",
        .kind = SymbolKind::Function,
        .deprecated = false,
        .range = toLSPRange(Lambda.range()),
        .selectionRange = getLambdaSelectionRage(Lambda),
        .children = std::move(Children),
    };
    Symbols.emplace_back(std::move(Sym));
    break;
  }
  case Node::NK_ExprList: {
    // Lambda, special formals.
    std::vector<DocumentSymbol> Children;
    const auto &List = static_cast<const ExprList &>(*AST);
    for (const Node *Ch : AST->children())
      collect(Ch, Children, VLA);

    DocumentSymbol Sym{
        .name = "{anonymous}",
        .detail = "list",
        .kind = SymbolKind::Array,
        .deprecated = false,
        .range = toLSPRange(List.range()),
        .selectionRange = toLSPRange(List.range()),
        .children = std::move(Children),
    };
    Symbols.emplace_back(std::move(Sym));
    break;
  }
  case Node::NK_ExprAttrs: {
    // Dispatch to "SemaAttrs", after desugaring.
    const SemaAttrs &SA = static_cast<const ExprAttrs &>(*AST).sema();
    for (const auto &[Name, Attr] : SA.staticAttrs()) {
      if (!Attr.value())
        continue;
      std::vector<DocumentSymbol> Children;
      collect(Attr.value(), Children, VLA);
      DocumentSymbol Sym{
          .name = Name,
          .detail = "attribute",
          .kind = SymbolKind::Field,
          .deprecated = false,
          .range = getAttrRange(Attr),
          .selectionRange = toLSPRange(Attr.key().range()),
          .children = std::move(Children),
      };
      Symbols.emplace_back(std::move(Sym));
    }
    for (const nixf::Attribute &Attr : SA.dynamicAttrs()) {
      std::vector<DocumentSymbol> Children;
      collect(Attr.value(), Children, VLA);
      DocumentSymbol Sym{
          .name = "${dynamic attribute}",
          .detail = "attribute",
          .kind = SymbolKind::Field,
          .deprecated = false,
          .range = getAttrRange(Attr),
          .selectionRange = toLSPRange(Attr.key().range()),
          .children = std::move(Children),
      };
      Symbols.emplace_back(std::move(Sym));
    }
    break;
  }
  default:
    // Trivial dispatch. Treat these symbol as same as this level.
    for (const Node *Ch : AST->children())
      collect(Ch, Symbols, VLA);
    break;
  }
}

} // namespace

void Controller::onDocumentSymbol(const DocumentSymbolParams &Params,
                                  Callback<std::vector<DocumentSymbol>> Reply) {
  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 this]() mutable {
    if (std::shared_ptr<NixTU> TU =
            getTU(URI.file().str(), Reply, /*Ignore=*/true)) {
      if (std::shared_ptr<Node> AST = getAST(*TU, Reply)) {
        std::vector<DocumentSymbol> Symbols;
        collect(AST.get(), Symbols, *TU->variableLookup());
        Reply(std::move(Symbols));
      }
    }
  };
  boost::asio::post(Pool, std::move(Action));
}
