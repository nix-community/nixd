#if NIX_IMPL == 1
#include <nix/expr/eval.hh>
#else
#include <eval.hh>
#endif

namespace nixt {

void evalExprFromCwd(nix::EvalState &State, std::string Src, nix::Value &V);

}
