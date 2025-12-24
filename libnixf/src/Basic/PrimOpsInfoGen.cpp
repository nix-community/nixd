#include <nix/expr/eval.hh>
#include <nix/expr/primops.hh>

#include <iostream>

int main(int argc, char **argv) {
  std::freopen(argv[1], "w", stdout);
  nix::initGC();
  // Generate .cpp file that inject all primops into a custom vector.
  // This is used by the language server to provide documentation,
  // but omit the implementation of primops.
  std::cout << "#include <nixf/Sema/PrimOpInfo.h>" << "\n\n";
  std::cout << "std::map<std::string, nixf::PrimOpInfo> nixf::PrimOpsInfo = {"
            << '\n';
  for (const auto &PrimOp : nix::RegisterPrimOp::primOps()) {
    std::cout << "  {" << "\"" << PrimOp.name << "\", {\n";

    // .args
    std::cout << "    .Args = {";
    for (size_t I = 0; I < PrimOp.args.size(); ++I) {
      std::cout << "\"" << PrimOp.args[I] << "\"";
      if (I + 1 < PrimOp.args.size()) {
        std::cout << ", ";
      }
    }
    std::cout << "},\n";

    // .arity
    std::cout << "    .Arity = " << PrimOp.arity << ",\n";

    // .doc
    std::cout << "    .Doc = R\"xabc(" << (PrimOp.doc ? PrimOp.doc : "")
              << ")xabc\",\n";

    // .internal
    std::cout << "    .Internal = " << (PrimOp.internal ? "true" : "false")
              << ",\n";

    std::cout << "  }},\n";
  }
  std::cout << "};\n";
  return 0;
}
