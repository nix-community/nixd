#include "nixd/Controller/NixTU.h"

using namespace nixd;

NixTU::NixTU(std::vector<nixf::Diagnostic> Diagnostics,
             std::shared_ptr<nixf::Node> AST,
             std::optional<util::OwnedRegion> ASTByteCode,
             std::unique_ptr<nixf::VariableLookupAnalysis> VLA)
    : Diagnostics(std::move(Diagnostics)), AST(std::move(AST)),
      ASTByteCode(std::move(ASTByteCode)), VLA(std::move(VLA)) {
  if (this->AST) {
    PMA = std::make_unique<nixf::ParentMapAnalysis>();
    PMA->runOnAST(*this->AST);
  }
}
