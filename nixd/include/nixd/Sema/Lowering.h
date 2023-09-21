#pragma once

#include "nixd/Sema/EvalContext.h"
#include "nixd/Syntax/Diagnostic.h"
#include "nixd/Syntax/Nodes.h"

#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>

namespace nixd {

struct Lowering;

class ExprAttrsBuilder {
  // The official parser does this work during the lowering process, via
  // @addAttr in src/libexpr/parser.y

  Lowering &LW;
  nix::ExprAttrs *Result;

  RangeIdx Range;

  bool Recursive;

  /// let ... in ...
  /// It is not allowed to use dynamic binds here, so we want to give diagnostic
  /// to each occurrence.
  bool IsLet;

  /// Nested attributes, we create a new builder for them, and collapse the map
  /// while finishing
  std::map<nix::Symbol, std::unique_ptr<ExprAttrsBuilder>> Nested;

  std::map<syntax::Node *, std::unique_ptr<ExprAttrsBuilder>> DynamicNested;

  // Use a map to detect duplicated fields
  // Not using the map inside nix::ExprAttrs because we want to preserve the
  // range
  std::map<nix::Symbol, const syntax::Node *> Fields;

public:
  ExprAttrsBuilder(Lowering &LW, RangeIdx Range, bool Recursive, bool IsLet);

  void addAttr(const syntax::Node *Attr, const syntax::Node *Body,
               bool Recursive);
  void addAttribute(const syntax::Attribute &A);
  void addBinds(const syntax::Binds &Binds);
  void addAttrSet(const syntax::AttrSet &AS);
  void addInherited(const syntax::InheritedAttribute &IA);
  nix::ExprAttrs *finish();
};

struct Lowering {
  nix::SymbolTable &STable;
  nix::PosTable &PTable;
  std::vector<syntax::Diagnostic> &Diags;
  EvalContext &Ctx;

  nix::Expr *lower(const syntax::Node *Root);
  nix::ExprLambda *lowerFunction(const syntax::Function *Fn);
  nix::Formal lowerFormal(const syntax::Formal &Formal);
  nix::AttrPath lowerAttrPath(const syntax::AttrPath &Path);
};

} // namespace nixd
