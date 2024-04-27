/// \file
/// \brief nixf-tidy, provide linting based on libnixf.

#include "nixf/Basic/JSONDiagnostic.h"
#include "nixf/Basic/Nodes/Basic.h"
#include "nixf/Parse/Parser.h"
#include "nixf/Sema/VariableLookup.h"

#include <iostream>
#include <sstream>

using namespace nixf;

namespace {

/// Pretty print the output.
bool PrettyPrint = false;

/// Enable variable lookup warnings.
bool VLA;

void parseArgs(int Argc, const char *Argv[]) {
  for (int I = 0; I < Argc; I++) {
    if (std::string_view(Argv[I]) == "--pretty-print")
      PrettyPrint = true;
    else if (std::string_view(Argv[I]) == "--variable-lookup")
      VLA = true;
  }
}

} // namespace

int main(int Argc, const char *Argv[]) {
  parseArgs(Argc, Argv);
  std::ostringstream Inputs;
  Inputs << std::cin.rdbuf();
  std::string Src = Inputs.str();

  std::vector<nixf::Diagnostic> Diags;
  std::shared_ptr<nixf::Node> AST = nixf::parse(Src, Diags);

  if (VLA) {
    VariableLookupAnalysis V(Diags);
    if (AST)
      V.runOnAST(*AST);
  }

  nlohmann::json V;
  to_json(V, Diags);

  if (PrettyPrint)
    std::cout << std::setw(4);
  std::cout << V << "\n";
  return 0;
}
