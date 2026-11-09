/// \file
/// \brief This implements [Rename].
/// [Rename]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_rename

#include "CheckReturn.h"
#include "Convert.h"
#include "Definition.h"

#include "lspserver/Protocol.h"
#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>

#include <algorithm>
#include <exception>

using namespace lspserver;
using namespace nixd;
using namespace llvm;
using namespace nixf;

namespace {

struct RenameException : std::exception {};

struct RenameWithException : RenameException {
  [[nodiscard]] const char *what() const noexcept override {
    return "cannot rename `with` defined variable";
  }
};

struct RenameBuiltinException : RenameException {
  [[nodiscard]] const char *what() const noexcept override {
    return "cannot rename builtin variable";
  }
};

struct RenameRecException : RenameException {
  [[nodiscard]] const char *what() const noexcept override {
    return "cannot rename `rec` attribute: renaming it renames the attribute "
           "the set exports, which its users select by name; rename it by "
           "hand";
  }
};

const Inherit *upInherit(const Node &Name, const ParentMapAnalysis &PMA) {
  const Node *Up = PMA.query(Name);
  if (Up && Up->kind() == Node::NK_Inherit)
    return static_cast<const Inherit *>(Up);
  return nullptr;
}

/// A name in `inherit` is both the attribute's name and the variable (or the
/// attribute of the inherited-from expression), so renaming one of them
/// rewrites it as its own binding \p Binding, keeping the inherit's other
/// names.
void splitInherit(const Inherit &Inh, const Node &Name, std::string Binding,
                  llvm::StringRef Src,
                  std::vector<lspserver::TextEdit> &Edits) {
  const auto &Names = Inh.names();
  if (Names.size() == 1) {
    Edits.emplace_back(lspserver::TextEdit{
        .range = toLSPRange(Src, Inh.range()),
        .newText = std::move(Binding),
    });
    return;
  }
  auto It = std::find_if(Names.begin(), Names.end(),
                         [&](const auto &N) { return N.get() == &Name; });
  assert(It != Names.end());
  LexerCursorRange Removal =
      std::next(It) != Names.end()
          ? LexerCursorRange{Name.lCur(), (*std::next(It))->lCur()}
          : LexerCursorRange{(*std::prev(It))->rCur(), Name.rCur()};
  Edits.emplace_back(lspserver::TextEdit{
      .range = toLSPRange(Src, Removal),
      .newText = "",
  });
  Edits.emplace_back(lspserver::TextEdit{
      .range = toLSPRange(Src, LexerCursorRange{Inh.rCur()}),
      .newText = " " + std::move(Binding),
  });
}

/// `inherit (E) name;` selects `name` from `E`.
std::string selectFrom(const Expr &E, llvm::StringRef Name,
                       llvm::StringRef Src) {
  std::string From(E.src(Src));
  bool Bare = E.kind() == Node::NK_ExprVar ||
              (E.kind() == Node::NK_ExprSelect &&
               !static_cast<const ExprSelect &>(E).defaultExpr());
  return (Bare ? From : "(" + From + ")") + "." + Name.str();
}

WorkspaceEdit rename(const nixf::Node &Desc, const std::string &NewText,
                     const ParentMapAnalysis &PMA,
                     const VariableLookupAnalysis &VLA, const URIForFile &URI,
                     llvm::StringRef Src) {
  using lspserver::TextEdit;
  // Find "definition"
  auto Def = findDefinition(Desc, PMA, VLA);

  if (Def.source() == Definition::DS_With)
    throw RenameWithException();

  if (Def.isBuiltin())
    throw RenameBuiltinException();

  if (Def.source() == Definition::DS_Rec)
    throw RenameRecException();

  std::vector<TextEdit> Edits;

  for (const auto *Use : Def.uses()) {
    // `inherit name;` desugars into a variable sharing the name's identifier.
    const Node *Name = PMA.query(Use->id());
    if (const Inherit *Inh = Name ? upInherit(*Name, PMA) : nullptr) {
      splitInherit(*Inh, *Name,
                   std::string(Name->src(Src)) + " = " + NewText + ";", Src,
                   Edits);
      continue;
    }
    Edits.emplace_back(TextEdit{
        .range = toLSPRange(Src, Use->range()),
        .newText = NewText,
    });
  }

  if (const Inherit *Inh = upInherit(*Def.syntax(), PMA)) {
    std::string Name(Def.syntax()->src(Src));
    std::string Value =
        Inh->expr() ? selectFrom(*Inh->expr(), Name, Src) : Name;
    splitInherit(*Inh, *Def.syntax(), NewText + " = " + Value + ";", Src,
                 Edits);
  } else {
    Edits.emplace_back(TextEdit{
        .range = toLSPRange(Src, Def.syntax()->range()),
        .newText = NewText,
    });
  }
  WorkspaceEdit WE;
  WE.changes = std::map<std::string, std::vector<TextEdit>>{
      {URI.uri(), std::move(Edits)}};
  return WE;
}
} // namespace

void Controller::onRename(const RenameParams &Params,
                          Callback<WorkspaceEdit> Reply) {
  using CheckTy = WorkspaceEdit;
  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 Pos = toNixfPosition(Params.position),
                 NewText = Params.newName, this]() mutable {
    const auto File = URI.file();
    return Reply([&]() -> llvm::Expected<WorkspaceEdit> {
      const auto TU = CheckDefault(getTU(File));
      const auto AST = CheckDefault(getAST(*TU));
      const auto &Desc = *CheckReturn(
          AST->descend({Pos, Pos}),
          error("cannot find corresponding node on given position"));

      const auto &PM = *TU->parentMap();
      const auto &VLA = *TU->variableLookup();
      try {
        return rename(Desc, NewText, PM, VLA, URI, TU->src());
      } catch (std::exception &E) {
        return error(E.what());
      }
    }());
  };
  boost::asio::post(Pool, std::move(Action));
}

void Controller::onPrepareRename(
    const lspserver::TextDocumentPositionParams &Params,
    Callback<std::optional<lspserver::Range>> Reply) {
  using CheckTy = std::optional<lspserver::Range>;
  auto Action = [Reply = std::move(Reply), URI = Params.textDocument.uri,
                 Pos = toNixfPosition(Params.position), this]() mutable {
    const auto File = URI.file();
    return Reply([&]() -> llvm::Expected<CheckTy> {
      const auto TU = CheckDefault(getTU(File));
      const auto AST = CheckDefault(getAST(*TU));

      const auto &Desc = *CheckDefault(AST->descend({Pos, Pos}));

      const auto &PM = *TU->parentMap();
      const auto &VLA = *TU->variableLookup();
      try {
        WorkspaceEdit WE = rename(Desc, "", PM, VLA, URI, TU->src());
        return toLSPRange(TU->src(), Desc.range());
      } catch (std::exception &E) {
        return error(E.what());
      }
    }());
  };
  boost::asio::post(Pool, std::move(Action));
}
