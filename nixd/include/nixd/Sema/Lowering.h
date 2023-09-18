#pragma once

#include "nixd/Sema/EvalContext.h"
#include "nixd/Syntax/Diagnostic.h"
#include "nixd/Syntax/Nodes.h"

#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>

namespace nixd {

struct Lowering {
  nix::SymbolTable &STable;
  nix::PosTable &PTable;
  std::vector<syntax::Diagnostic> &Diags;

  nix::Expr *lower(EvalContext &Ctx, syntax::Node *Root);
  nix::ExprLambda *lowerFunction(EvalContext &Ctx, syntax::Function *Fn);
  nix::Formal lowerFormal(EvalContext &Ctx, const syntax::Formal &Formal);
};

} // namespace nixd
