/// \file
/// \brief Implementation of static attribute resolution.

#include "AttrResolve.h"
#include "PathResolve.h"

#include "nixf/Basic/Nodes/Expr.h"
#include "nixf/Basic/Nodes/Lambda.h"
#include "nixf/Basic/Nodes/Op.h"
#include "nixf/Basic/Nodes/Simple.h"

#include <filesystem>

using namespace nixd;
using namespace nixf;

namespace {

/// Resolution follows bindings, so `let a = a; in a.x` would never end.
constexpr int MaxDepth = 64;

struct DepthGuard {
  int &Depth;
  explicit DepthGuard(int &Depth) : Depth(Depth) { ++Depth; }
  ~DepthGuard() { --Depth; }
};

std::string canonical(const std::string &Path) {
  std::error_code EC;
  auto P = std::filesystem::weakly_canonical(Path, EC);
  return EC ? Path : P.string();
}

} // namespace

AttrResolver::AttrResolver(std::string Path, std::shared_ptr<const NixTU> TU,
                           Loader Load)
    : Load(std::move(Load)) {
  auto F = std::make_unique<File>(File{Path, std::move(TU)});
  Main = F.get();
  Files.emplace(canonical(Path), std::move(F));
}

const AttrResolver::File *AttrResolver::file(const std::string &Path) {
  auto [It, Inserted] = Files.try_emplace(Path);
  if (Inserted) {
    std::string Opened = Path;
    std::shared_ptr<const NixTU> TU = Load(Opened);
    if (TU && TU->ast() && TU->variableLookup() && TU->parentMap())
      It->second = std::make_unique<File>(File{Opened, std::move(TU)});
  }
  return It->second.get();
}

std::optional<AttrResolver::Found> AttrResolver::lookup(const Set &S,
                                                        const AttrName *Name) {
  if (!Name || !Name->isStatic())
    return std::nullopt;
  const auto &Static = S.Attrs->sema().staticAttrs();
  // A `rec` set's `__overrides` replaces its attributes when it is evaluated.
  if (S.Attrs->sema().isRecursive() && Static.contains("__overrides"))
    return std::nullopt;
  auto It = Static.find(Name->staticName());
  if (It == Static.end())
    return std::nullopt;
  return Found{&It->second, S.In};
}

std::optional<AttrResolver::Found>
AttrResolver::bindingOf(const Definition &Def, const File &F) {
  if (Def.source() != Definition::DS_Let && Def.source() != Definition::DS_Rec)
    return std::nullopt;
  const Node *Syntax = Def.syntax();
  if (!Syntax || Syntax->kind() != Node::NK_AttrName)
    return std::nullopt;
  const auto &Name = static_cast<const AttrName &>(*Syntax);
  // `s.value = ...;` merges `value` into `s`: it is not a binding of the set
  // it is written in.
  const Node *Up = F.TU->parentMap()->query(Name);
  if (Up && Up->kind() == Node::NK_AttrPath)
    return keyAt(Name, F);
  const Node *Attrs = F.TU->parentMap()->upTo(*Syntax, Node::NK_ExprAttrs);
  if (!Attrs)
    return std::nullopt;
  return lookup({static_cast<const ExprAttrs *>(Attrs), &F},
                static_cast<const AttrName *>(Syntax));
}

void AttrResolver::selectEach(
    const Expr *Base, const std::vector<std::shared_ptr<AttrName>> &Names,
    const File &F,
    const std::function<void(const AttrName &, const Found &)> &Step) {
  std::optional<Set> S = resolveSet(Base, F);
  std::optional<Found> Prev;
  for (const auto &Name : Names) {
    if (Prev)
      S = resolveSet(Prev->Attr->value(), *Prev->In);
    if (!S)
      return;
    Prev = lookup(*S, Name.get());
    if (!Prev)
      return;
    Step(*Name, *Prev);
  }
}

std::optional<AttrResolver::Set>
AttrResolver::resolveImport(const ExprCall &Call, const File &F) {
  if (Call.fn().kind() != Node::NK_ExprVar)
    return std::nullopt;
  const auto &Fn = static_cast<const ExprVar &>(Call.fn());
  auto R = F.TU->variableLookup()->query(Fn);
  if (R.Kind != VariableLookupAnalysis::LookupResultKind::Defined || !R.Def ||
      !R.Def->isBuiltin() || Fn.id().name() != "import")
    return std::nullopt;

  const auto &Args = Call.args();
  if (Args.empty() || !Args[0] || Args[0]->kind() != Node::NK_ExprPath)
    return std::nullopt;
  const auto &Path = static_cast<const ExprPath &>(*Args[0]);
  if (!Path.parts().isLiteral())
    return std::nullopt;
  auto Resolved = resolveExprPath(F.Path, Path.parts().literal());
  if (!Resolved)
    return std::nullopt;
  const File *Imported = file(*Resolved);
  if (!Imported)
    return std::nullopt;

  // `import ./f.nix a b` applies the file's top lambdas to `a` and `b`.
  const Node *Top = Imported->TU->ast().get();
  for (std::size_t I = 1; I < Args.size(); ++I) {
    if (Top->kind() != Node::NK_ExprLambda)
      return std::nullopt;
    Top = static_cast<const ExprLambda *>(Top)->body();
    if (!Top)
      return std::nullopt;
  }
  if (!Expr::isExpr(Top->kind()))
    return std::nullopt;
  return resolveSet(static_cast<const Expr *>(Top), *Imported);
}

std::optional<AttrResolver::Set> AttrResolver::resolveSet(const Expr *E,
                                                          const File &F) {
  DepthGuard G(Depth);
  if (!E || Depth > MaxDepth)
    return std::nullopt;
  switch (E->kind()) {
  case Node::NK_ExprAttrs:
    return Set{static_cast<const ExprAttrs *>(E), &F};
  case Node::NK_ExprParen:
    return resolveSet(static_cast<const ExprParen *>(E)->expr(), F);
  case Node::NK_ExprLet:
    return resolveSet(static_cast<const ExprLet *>(E)->expr(), F);
  case Node::NK_ExprVar: {
    auto R = F.TU->variableLookup()->query(*static_cast<const ExprVar *>(E));
    if (R.Kind != VariableLookupAnalysis::LookupResultKind::Defined || !R.Def)
      return std::nullopt;
    if (auto B = bindingOf(*R.Def, F))
      return resolveSet(B->Attr->value(), *B->In);
    return std::nullopt;
  }
  case Node::NK_ExprSelect: {
    const auto &Sel = static_cast<const ExprSelect &>(*E);
    if (!Sel.path())
      return std::nullopt;
    const auto &Names = Sel.path()->names();
    std::size_t Selected = 0;
    std::optional<Found> Last;
    selectEach(&Sel.expr(), Names, F, [&](const AttrName &, const Found &A) {
      ++Selected;
      Last = A;
    });
    if (Selected != Names.size())
      return std::nullopt;
    return resolveSet(Last->Attr->value(), *Last->In);
  }
  case Node::NK_ExprCall:
    return resolveImport(static_cast<const ExprCall &>(*E), F);
  default:
    return std::nullopt;
  }
}

std::optional<AttrResolver::Found> AttrResolver::keyAt(const AttrName &Name,
                                                       const File &F) {
  const ParentMapAnalysis &PM = *F.TU->parentMap();
  const auto &Path = static_cast<const AttrPath &>(*PM.query(Name));
  std::vector<const AttrName *> Names;
  for (const auto &N : Path.names()) {
    Names.emplace_back(N.get());
    if (N.get() == &Name)
      break;
  }

  // Start from the outermost set: `{ a = { b = 1; }; a.c = 2; }` merges both
  // values of `a` into the first, so `b` and `c` are found through `a`.
  const Node *Attrs = PM.upTo(*PM.query(Path), Node::NK_ExprAttrs);
  while (Attrs) {
    const Node *Up = PM.query(*Attrs);
    if (!Up || Up->kind() != Node::NK_Binding)
      break;
    const auto &B = static_cast<const Binding &>(*Up);
    if (B.value().get() != Attrs)
      break;
    const auto &Outer = B.path().names();
    std::vector<const AttrName *> Prefix;
    Prefix.reserve(Outer.size());
    for (const auto &N : Outer)
      Prefix.emplace_back(N.get());
    Names.insert(Names.begin(), Prefix.begin(), Prefix.end());
    Attrs = PM.upTo(B, Node::NK_ExprAttrs);
  }
  if (!Attrs)
    return std::nullopt;

  std::optional<Set> S = Set{static_cast<const ExprAttrs *>(Attrs), &F};
  std::optional<Found> Cur;
  for (const AttrName *N : Names) {
    if (Cur)
      S = resolveSet(Cur->Attr->value(), *Cur->In);
    if (!S)
      return std::nullopt;
    Cur = lookup(*S, N);
    if (!Cur)
      return std::nullopt;
  }
  return Cur;
}

std::optional<std::pair<AttrResolver::Found, AttrResolver::Role>>
AttrResolver::attrAt(const Node &N) {
  const ParentMapAnalysis &PM = *Main->TU->parentMap();
  const Node *UpExpr = PM.upExpr(N);
  if (UpExpr && UpExpr->kind() == Node::NK_ExprVar) {
    auto R = Main->TU->variableLookup()->query(
        static_cast<const ExprVar &>(*UpExpr));
    if (R.Kind != VariableLookupAnalysis::LookupResultKind::Defined || !R.Def ||
        R.Def->source() != Definition::DS_Rec)
      return std::nullopt;
    if (auto B = bindingOf(*R.Def, *Main))
      return std::pair{*B, Role::Variable};
    return std::nullopt;
  }

  const Node *Name = PM.upTo(N, Node::NK_AttrName);
  if (!Name)
    return std::nullopt;
  const auto &AN = static_cast<const AttrName &>(*Name);
  const Node *Up = PM.query(AN);
  if (!Up)
    return std::nullopt;

  if (Up->kind() == Node::NK_Inherit) {
    const auto &Inh = static_cast<const Inherit &>(*Up);
    if (!Inh.expr())
      return std::nullopt;
    auto S = resolveSet(Inh.expr().get(), *Main);
    if (!S)
      return std::nullopt;
    if (auto A = lookup(*S, &AN))
      return std::pair{*A, Role::Selection};
    return std::nullopt;
  }

  if (Up->kind() != Node::NK_AttrPath)
    return std::nullopt;
  const auto &Path = static_cast<const AttrPath &>(*Up);
  const Node *Owner = PM.query(Path);
  if (!Owner)
    return std::nullopt;

  const Expr *Base = nullptr;
  switch (Owner->kind()) {
  case Node::NK_Binding:
    if (auto A = keyAt(AN, *Main))
      return std::pair{*A, Role::Key};
    return std::nullopt;
  case Node::NK_ExprSelect:
    Base = &static_cast<const ExprSelect &>(*Owner).expr();
    break;
  case Node::NK_ExprOpHasAttr:
    Base = static_cast<const ExprOpHasAttr &>(*Owner).expr();
    break;
  default:
    return std::nullopt;
  }

  std::optional<Found> Result;
  selectEach(Base, Path.names(), *Main,
             [&](const AttrName &Sel, const Found &A) {
               if (&Sel == &AN)
                 Result = A;
             });
  if (Result)
    return std::pair{*Result, Role::Selection};
  return std::nullopt;
}

void AttrResolver::collect(const Node *N, const Found &Target,
                           std::vector<const Node *> &Out) {
  if (!N)
    return;
  auto Match = [&](const AttrName &Name, const Found &A) {
    if (&A.key() == &Target.key())
      Out.emplace_back(&Name);
  };
  switch (N->kind()) {
  case Node::NK_ExprSelect: {
    const auto &Sel = static_cast<const ExprSelect &>(*N);
    if (Sel.path())
      selectEach(&Sel.expr(), Sel.path()->names(), *Main, Match);
    break;
  }
  case Node::NK_ExprOpHasAttr: {
    const auto &Has = static_cast<const ExprOpHasAttr &>(*N);
    if (Has.attrpath())
      selectEach(Has.expr(), Has.attrpath()->names(), *Main, Match);
    break;
  }
  case Node::NK_Inherit: {
    const auto &Inh = static_cast<const Inherit &>(*N);
    if (!Inh.expr())
      break;
    if (auto S = resolveSet(Inh.expr().get(), *Main))
      for (const auto &Name : Inh.names())
        if (auto A = lookup(*S, Name.get()))
          Match(*Name, *A);
    break;
  }
  default:
    break;
  }
  for (const Node *Child : N->children())
    collect(Child, Target, Out);
}

std::vector<const Node *> AttrResolver::references(const Found &Target) {
  std::vector<const Node *> Out;
  if (Target.In == Main)
    if (const Definition *Def = Main->TU->variableLookup()->toDef(Target.key()))
      for (const ExprVar *Use : Def->uses())
        Out.emplace_back(Use);
  collect(Main->TU->ast().get(), Target, Out);
  return Out;
}
