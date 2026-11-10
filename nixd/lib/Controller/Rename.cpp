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
#include <set>

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

struct RenameInheritSharedException : RenameException {
  [[nodiscard]] const char *what() const noexcept override {
    return "cannot rename: the binding that replaces the name would copy the "
           "expression of `inherit (...)`, which the other names keep "
           "reading; the copy would be evaluated again, as a value of its own";
  }
};

struct RenameCurPosException : RenameException {
  [[nodiscard]] const char *what() const noexcept override {
    return "cannot rename `__curPos`: written as an expression it is the "
           "current position, not this binding";
  }
};

struct RenameInheritMovesCurPosException : RenameException {
  [[nodiscard]] const char *what() const noexcept override {
    return "cannot rename: the binding that replaces the name would copy the "
           "expression of `inherit (...)`, and `__curPos` in it is the "
           "position it is written at";
  }
};

struct RenameInheritCaptureException : RenameException {
  [[nodiscard]] const char *what() const noexcept override {
    return "cannot rename: `inherit` reads the variable from outside its "
           "`rec` or `let`, but the binding that replaces it would read the "
           "new name from that `rec` or `let`, which binds it";
  }
};

struct RenameInheritNestedException : RenameException {
  [[nodiscard]] const char *what() const noexcept override {
    return "cannot rename: the variable is inherited inside the expression of "
           "an `inherit (...)` that has to be rewritten";
  }
};

struct RenameRecException : RenameException {
  [[nodiscard]] const char *what() const noexcept override {
    return "cannot rename `rec` attribute: renaming it renames the attribute "
           "the set exports, which its users select by name; rename it by "
           "hand";
  }
};

/// The inherit that \p Name is one of the names of.
const Inherit *upInherit(const Node &Name, const ParentMapAnalysis &PMA) {
  // The expression of `inherit (E) a;` is a child of the inherit too.
  if (Name.kind() != Node::NK_AttrName)
    return nullptr;
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
  // The name and the spaces before it on its line: comments stay.
  LexerCursor L = Name.lCur();
  std::size_t K = 0;
  while (K < static_cast<std::size_t>(L.column()) &&
         (Src[L.offset() - K - 1] == ' ' || Src[L.offset() - K - 1] == '\t'))
    ++K;
  LexerCursorRange Removal{
      LexerCursor::unsafeCreate(L.line(), L.column() - K, L.offset() - K),
      Name.rCur()};
  // Names need no space between them when one is quoted: removing `"b"` from
  // `a"b"c` must not join `a` and `c` into `ac`.
  auto Space = [](char C) {
    return C == ' ' || C == '\t' || C == '\n' || C == '\r';
  };
  std::size_t Begin = Removal.lCur().offset(), End = Removal.rCur().offset();
  bool Glues = Begin > 0 && End < Src.size() && !Space(Src[Begin - 1]) &&
               !Space(Src[End]) && Src[End] != ';';
  Edits.emplace_back(lspserver::TextEdit{
      .range = toLSPRange(Src, Removal),
      .newText = Glues ? " " : "",
  });
  Edits.emplace_back(lspserver::TextEdit{
      .range = toLSPRange(Src, LexerCursorRange{Inh.rCur()}),
      .newText = " " + std::move(Binding),
  });
}

/// The name a definition binds: a quoted `inherit "x";` binds `x`.
std::string definedName(const Node &Syntax, llvm::StringRef Src) {
  if (Syntax.kind() == Node::NK_AttrName) {
    const auto &Name = static_cast<const AttrName &>(Syntax);
    if (Name.isStatic())
      return Name.staticName();
  }
  return std::string(Syntax.src(Src));
}

/// Whether \p N mentions `__curPos`, whose value is the position it is
/// written at.
bool mentionsCurPos(const Node &N) {
  if (N.kind() == Node::NK_ExprVar &&
      static_cast<const ExprVar &>(N).id().name() == "__curPos")
    return true;
  for (const Node *Child : N.children())
    if (Child && mentionsCurPos(*Child))
      return true;
  return false;
}

/// Whether copying \p E refers to the value it denotes rather than a new one:
/// a variable, or attributes selected from one.
bool isShared(const Expr &E) {
  switch (E.kind()) {
  case Node::NK_ExprVar:
    return true;
  case Node::NK_ExprParen: {
    const Expr *Inner = static_cast<const ExprParen &>(E).expr();
    return Inner && isShared(*Inner);
  }
  case Node::NK_ExprSelect: {
    const auto &Sel = static_cast<const ExprSelect &>(E);
    return !Sel.defaultExpr() && isShared(Sel.expr());
  }
  default:
    return false;
  }
}

bool contains(const Node &Outer, const Node &N) {
  return Outer.lCur().offset() <= N.lCur().offset() &&
         N.rCur().offset() <= Outer.rCur().offset();
}

/// `inherit (E) name;` selects `name` from `E`, whose text is \p From: the
/// source of `E` with the renamed uses inside it already replaced.
std::string selectFrom(const Expr &E, const std::string &From,
                       llvm::StringRef Name) {
  bool Bare = E.kind() == Node::NK_ExprVar ||
              (E.kind() == Node::NK_ExprSelect &&
               !static_cast<const ExprSelect &>(E).defaultExpr());
  return (Bare ? From : "(" + From + ")") + "." + Name.str();
}

/// The source of \p E, with each of \p Uses (all inside it) renamed.
std::string renamedSource(const Expr &E, std::vector<const ExprVar *> Uses,
                          const std::string &NewText, llvm::StringRef Src) {
  std::string Text(E.src(Src));
  std::sort(Uses.begin(), Uses.end(), [](const auto *A, const auto *B) {
    return A->lCur().offset() > B->lCur().offset();
  });
  for (const ExprVar *Use : Uses) {
    auto Begin = Use->lCur().offset() - E.lCur().offset();
    Text.replace(Begin, Use->rCur().offset() - Use->lCur().offset(), NewText);
  }
  return Text;
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

  // The expression `__curPos` is the position it is written at, however a
  // binding of that name is spelled (`inherit "__curPos";` too), so what
  // looks like its uses are not, and a rename would change them.
  if (definedName(*Def.syntax(), Src) == "__curPos")
    throw RenameCurPosException();

  // Renaming to the current name changes nothing, but splitting `inherit x;`
  // would write `x = x;`, which recurses.
  if (definedName(*Def.syntax(), Src) == NewText) {
    WorkspaceEdit WE;
    WE.changes = std::map<std::string, std::vector<TextEdit>>{{URI.uri(), {}}};
    return WE;
  }

  std::vector<TextEdit> Edits;

  // A definition from `inherit (E) name;` is rewritten as a binding that
  // copies `E`, so uses inside `E` are renamed in that copy.
  const Inherit *DefInh = upInherit(*Def.syntax(), PMA);
  const Expr *From = DefInh ? DefInh->expr().get() : nullptr;
  if (From && DefInh->names().size() > 1 && !isShared(*From))
    throw RenameInheritSharedException();
  std::vector<const ExprVar *> UsesInFrom;

  // `inherit (E) a b;` desugars into one selection per name, all sharing `E`,
  // so a use inside `E` is recorded once per name.
  std::set<const ExprVar *> Seen;
  for (const auto *Use : Def.uses()) {
    if (!Seen.insert(Use).second)
      continue;
    // `inherit name;` desugars into a variable sharing the name's identifier.
    const Node *Name = PMA.query(Use->id());
    const Inherit *Inh = Name ? upInherit(*Name, PMA) : nullptr;
    if (From && contains(*From, *Use)) {
      if (Inh)
        throw RenameInheritNestedException();
      UsesInFrom.emplace_back(Use);
      // A single-name inherit is replaced whole, `E` included.
      if (DefInh->names().size() == 1)
        continue;
    } else if (Inh) {
      const Node *Attrs = PMA.upTo(*Inh, Node::NK_ExprAttrs);
      if (Attrs) {
        const SemaAttrs &SA = static_cast<const ExprAttrs &>(*Attrs).sema();
        if (SA.isRecursive() && SA.staticAttrs().contains(NewText))
          throw RenameInheritCaptureException();
      }
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

  if (DefInh) {
    std::string Name(Def.syntax()->src(Src));
    if (From && mentionsCurPos(*From))
      throw RenameInheritMovesCurPosException();
    std::string Value =
        From ? selectFrom(*From, renamedSource(*From, UsesInFrom, NewText, Src),
                          Name)
             : Name;
    splitInherit(*DefInh, *Def.syntax(), NewText + " = " + Value + ";", Src,
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
