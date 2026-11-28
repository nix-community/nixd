/// \file
/// \brief This implements [Rename].
/// [Rename]:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_rename

#include "AST.h"
#include "CheckReturn.h"
#include "CodeActions/Utils.h"
#include "Convert.h"
#include "Definition.h"

#include "lspserver/Protocol.h"
#include "nixd/Controller/Controller.h"

#include <boost/asio/post.hpp>

#include <algorithm>
#include <exception>
#include <functional>
#include <set>
#include <string>

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
  LexerCursorRange Removal = withLeadingSpaces(Name, Src);
  // Names need no space between them when one is quoted: removing `"b"` from
  // `a"b"c` must not join `a` and `c` into `ac`.
  Edits.emplace_back(lspserver::TextEdit{
      .range = toLSPRange(Src, Removal),
      .newText = gluesNeighbours(Removal, Src) ? " " : "",
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

struct RenameCaptureException : RenameException {
  std::string Message;
  explicit RenameCaptureException(std::string Message)
      : Message(std::move(Message)) {}
  [[nodiscard]] const char *what() const noexcept override {
    return Message.c_str();
  }
};

std::string at(const Node &N) {
  return "line " + std::to_string(N.lCur().line() + 1) + ", column " +
         std::to_string(N.lCur().column() + 1);
}

/// What the name \p Name, defined by \p D, is: for an error message.
std::string describe(const Definition &D, const std::string &Name) {
  if (D.isBuiltin())
    return "the builtin `" + Name + "`";
  if (D.source() == Definition::DS_With)
    return "`" + Name + "` from the `with` at " + at(*D.syntax());
  return "the `" + Name + "` bound at " + at(*D.syntax());
}

/// Whether \p E binds \p Name lexically. A `with` binds nothing lexically: a
/// lexical binding outside it wins over its attributes.
bool binds(const EnvNode &E, const std::string &Name) {
  return !E.isWith() && E.defs().contains(Name);
}

bool defines(const EnvNode &E, const std::string &Name, const Definition &D) {
  auto It = E.defs().find(Name);
  return It != E.defs().end() && It->second.get() == &D;
}

/// The environment \p D, named \p Name, is defined in.
const EnvNode *defEnv(const Definition &D, const std::string &Name,
                      const ParentMapAnalysis &PMA,
                      const VariableLookupAnalysis &VLA) {
  auto Up = [&](const EnvNode *E) -> const EnvNode * {
    for (; E; E = E->parent())
      if (defines(*E, Name, D))
        return E;
    return nullptr;
  };
  for (const ExprVar *Use : D.uses())
    if (const EnvNode *E = Up(VLA.env(Use)))
      return E;
  // Without uses, the environment of the scope's body.
  const Node *Body = nullptr;
  switch (D.source()) {
  case Definition::DS_Let:
    if (const Node *Attrs = PMA.upTo(*D.syntax(), Node::NK_ExprAttrs))
      if (const Node *Let = PMA.query(*Attrs);
          Let && Let->kind() == Node::NK_ExprLet)
        Body = static_cast<const ExprLet &>(*Let).expr();
    break;
  case Definition::DS_LambdaArg:
  case Definition::DS_LambdaNoArg_Formal:
  case Definition::DS_LambdaWithArg_Arg:
  case Definition::DS_LambdaWithArg_Formal:
  case Definition::DS_FlakeInjectedFormal:
    if (const Node *Lambda = PMA.upTo(*D.syntax(), Node::NK_ExprLambda))
      Body = static_cast<const ExprLambda &>(*Lambda).body();
    break;
  default:
    break;
  }
  return Body ? Up(VLA.env(Body)) : nullptr;
}

/// Calls \p F on every variable, those that `inherit name;` desugars into
/// included.
void forEachVar(const Node &N, const std::function<void(const ExprVar &)> &F) {
  if (N.kind() == Node::NK_ExprVar)
    F(static_cast<const ExprVar &>(N));
  if (N.kind() == Node::NK_ExprAttrs)
    for (const auto &[_, Attr] :
         static_cast<const ExprAttrs &>(N).sema().staticAttrs())
      if (Attr.kind() == Attribute::AttributeKind::Inherit && Attr.value() &&
          Attr.value()->kind() == Node::NK_ExprVar)
        F(static_cast<const ExprVar &>(*Attr.value()));
  for (const Node *Child : N.children())
    if (Child)
      forEachVar(*Child, F);
}

/// Refuses a rename of \p D from \p Old to \p New that would change what a
/// variable reads: a binding of \p New between a use and \p D would capture
/// the use, and the renamed binding would capture a use of \p New that reads
/// something outside it.
void checkCapture(const Definition &D, const std::string &Old,
                  const std::string &New, const Node &Root,
                  const ParentMapAnalysis &PMA,
                  const VariableLookupAnalysis &VLA) {
  if (New.empty())
    return;
  // A variable is named by an identifier: `"y"` would bind `y` but be written
  // as a string where the variable is used.
  if (!isValidNixIdentifier(New))
    throw RenameCaptureException("cannot rename: `" + New +
                                 "` is not a valid identifier");
  // `__curPos` is the position it is written at, never a variable.
  if (New == "__curPos")
    throw RenameCaptureException(
        "cannot rename: `__curPos` is the current position, not a variable");
  const EnvNode *DefE = defEnv(D, Old, PMA, VLA);
  if (DefE && binds(*DefE, New))
    throw RenameCaptureException(
        "cannot rename: " + describe(*DefE->defs().at(New), New) +
        " is in the same scope");

  for (const ExprVar *Use : D.uses())
    for (const EnvNode *E = VLA.env(Use); E && !defines(*E, Old, D);
         E = E->parent())
      if (binds(*E, New))
        throw RenameCaptureException(
            "cannot rename: the use of `" + Old + "` at " + at(*Use) +
            " would read " + describe(*E->defs().at(New), New) + " instead");

  if (!DefE)
    return;
  forEachVar(Root, [&](const ExprVar &Var) {
    if (Var.id().name() != New)
      return;
    for (const EnvNode *E = VLA.env(&Var); E; E = E->parent()) {
      if (binds(*E, New))
        return;
      if (E != DefE)
        continue;
      auto R = VLA.query(Var);
      std::string Reads =
          R.Def ? describe(*R.Def, New) : "`" + New + "`, which is undefined";
      throw RenameCaptureException(
          "cannot rename: the use of `" + New + "` at " + at(Var) + " reads " +
          Reads + ", but would read the renamed `" + Old + "`");
    }
  });
}

WorkspaceEdit rename(const nixf::Node &Desc, const std::string &NewText,
                     const ParentMapAnalysis &PMA,
                     const VariableLookupAnalysis &VLA, const URIForFile &URI,
                     llvm::StringRef Src) {
  using lspserver::TextEdit;
  // Find "definition"
  const Definition &Def = findDefinition(Desc, PMA, VLA);

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

  const Node *Root = &Desc;
  while (!PMA.isRoot(*Root) && PMA.query(*Root))
    Root = PMA.query(*Root);

  checkCapture(Def, definedName(*Def.syntax(), Src), NewText, *Root, PMA, VLA);

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
