#include <nix/eval-inline.hh>
#include <nix/shared.hh>

namespace nix {

// Copy-paste from nix source code, do not know why it is inlined.

inline void EvalState::evalAttrs(Env &env, Expr *e, Value &v, const PosIdx pos,
                                 std::string_view errorCtx) {
  try {
    e->eval(*this, env, v);
    if (v.type() != nAttrs)
      error("value is %1% while a set was expected", showType(v))
          .withFrame(env, *e)
          .debugThrow<TypeError>();
  } catch (Error &e) {
    e.addTrace(positions[pos], errorCtx);
    throw;
  }
}

} // namespace nix
