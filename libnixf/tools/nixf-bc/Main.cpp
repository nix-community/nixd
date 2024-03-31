#include "nixf/Basic/Diagnostic.h"
#include "nixf/Bytecode/Write.h"
#include "nixf/Parse/Parser.h"

#include <unistd.h>

#include <iostream>
#include <sstream>

int main() {
  std::ostringstream Inputs;
  Inputs << std::cin.rdbuf();
  std::string Src = Inputs.str();

  std::vector<nixf::Diagnostic> Diags;
  std::shared_ptr<nixf::Node> AST = nixf::parse(Src, Diags);
  nixf::writeBytecode(std::cout, AST.get());
  return 0;
}
