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
  EvalContext &Ctx;

  nix::Expr *lower(syntax::Node *Root);
  nix::ExprLambda *lowerFunction(syntax::Function *Fn);
  nix::Formal lowerFormal(const syntax::Formal &Formal);
};

} // namespace nixd
