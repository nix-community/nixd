#include "canon-path.hh"
#include "nixd/Expr.h"

#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>

#include <iostream>
#include <memory>

class InitNix {
public:
  InitNix() {
    nix::initNix();
    nix::initGC();
  }
  std::unique_ptr<nix::EvalState> getDummyState() {
    return std::make_unique<nix::EvalState>(nix::Strings{},
                                            nix::openStore("dummy://"));
  }
};

struct ASTDump : nixd::RecursiveASTVisitor<ASTDump> {

  int Depth = 0;
  std::unique_ptr<nix::EvalState> State;

  bool traverseExpr(const nix::Expr *E) {
    Depth++;
    if (!nixd::RecursiveASTVisitor<ASTDump>::traverseExpr(E))
      return false;
    Depth--;
    return true;
  }

#define NIX_EXPR(EXPR)                                                         \
  bool visit##EXPR(const nix::EXPR *E) {                                       \
    for (int i = 0; i < Depth; i++) {                                          \
      std::cout << " ";                                                        \
    }                                                                          \
    std::cout << #EXPR << ": ";                                                \
    E->show(State->symbols, std::cout);                                        \
    std::cout << "\n";                                                         \
    return true;                                                               \
  }
#include "nixd/NixASTNodes.inc"
#undef NIX_EXPR
};

int main(int argc, char *argv[]) {
  InitNix I;
  auto State = I.getDummyState();
  const auto *E = State->parseExprFromFile(nix::CanonPath(argv[1]));
  ASTDump A;
  A.State = std::move(State);
  A.traverseExpr(E);
  return 0;
}
