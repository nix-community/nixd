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

  // Use a map to detect duplicated fields
  // Not using the map inside nix::ExprAttrs because we want to preserve the
  // range
  std::map<nix::Symbol, syntax::Node *> Fields;

public:
  ExprAttrsBuilder(Lowering &LW);
  void addAttribute(const syntax::Attribute &A);
  void addInherited(const syntax::InheritedAttribute &IA);
  nix::ExprAttrs *finish();
};

struct Lowering {
  nix::SymbolTable &STable;
  nix::PosTable &PTable;
  std::vector<syntax::Diagnostic> &Diags;
  EvalContext &Ctx;

  nix::Expr *lower(syntax::Node *Root);
  nix::ExprLambda *lowerFunction(syntax::Function *Fn);
  nix::Formal lowerFormal(const syntax::Formal &Formal);
  nix::AttrPath lowerAttrPath(const syntax::AttrPath &Path);
  nix::ExprAttrs *lowerBinds(const syntax::Binds &Binds);
};

} // namespace nixd
