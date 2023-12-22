#include "nixd/Expr/Expr.h"

#include <nixt/Name.h>

#include <nix/canon-path.hh>
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

struct ASTDump : nixt::RecursiveASTVisitor<ASTDump> {

  int Depth = 0;
  std::unique_ptr<nix::EvalState> State;

  bool traverseExpr(const nix::Expr *E) {
    Depth++;
    if (!nixt::RecursiveASTVisitor<ASTDump>::traverseExpr(E))
      return false;
    Depth--;
    return true;
  }

  bool visitExpr(const nix::Expr *E) const {
    for (int I = 0; I < Depth; I++)
      std::cout << " ";
    std::cout << nixt::nameOf(E) << ": ";
    E->show(State->symbols, std::cout);
    std::cout << "\n";
    return true;
  }
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
