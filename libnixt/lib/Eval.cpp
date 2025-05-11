#include "nixt/Eval.h"

namespace {

nix::SourcePath getSourcePath(nix::EvalState &State, const std::string &Path) {
  return {nix::CanonPath::fromCwd()};
}

} // namespace

void nixt::evalExprFromCwd(nix::EvalState &State, std::string Src,
                           nix::Value &V) {
  State.eval(State.parseExprFromString(Src, getSourcePath(State, ".")), V);
}
