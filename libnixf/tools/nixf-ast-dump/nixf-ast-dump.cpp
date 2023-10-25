#include "nixf/Basic/Diagnostic.h"
#include "nixf/Basic/DiagnosticEngine.h"
#include "nixf/Lex/Lexer.h"
#include "nixf/Parse/Parser.h"

#include <iostream>
#include <sstream>

int main(int argc, char *argv[]) {
  std::stringstream SS;
  SS << std::cin.rdbuf();
  std::string Src = SS.str();
  nixf::DiagnosticEngine Diag;
  nixf::Lexer L(Src, Diag);
  nixf::Parser P(L, Diag);

  std::shared_ptr<nixf::RawNode> Expr = P.parseExpr();

  Expr->dumpAST(std::cout);
}
