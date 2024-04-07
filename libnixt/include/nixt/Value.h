#include <nix/eval.hh>

namespace nixt {

bool isOption(nix::EvalState &State, nix::Value &V);

bool isDerivation(nix::EvalState &State, nix::Value &V);

std::string attrPathStr(nix::EvalState &State, nix::Value &V,
                        const std::string &AttrPath);

} // namespace nixt
