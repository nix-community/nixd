#include "nixd/Nix/EvalState.h"

namespace nix {

void forceValueDepth(EvalState &State, Value &v, int depth) {
  std::set<const Value *> seen;

  std::function<void(Value & v, int Depth)> recurse;

  recurse = [&](Value &v, int depth) {
    if (depth == 0)
      return;
    if (!seen.insert(&v).second)
      return;

    State.forceValue(v, [&]() { return v.determinePos(noPos); });

    if (v.type() == nAttrs) {
      for (auto &i : *v.attrs)
        try {
          // If the value is a thunk, we're evaling. Otherwise no trace
          // necessary.
          recurse(*i.value, depth - 1);
        } catch (Error &e) {
          State.addErrorTrace(e, i.pos, "while evaluating the attribute '%1%'",
                              State.symbols[i.name]);
          throw;
        }
    }

    else if (v.isList()) {
      for (auto v2 : v.listItems())
        recurse(*v2, depth - 1);
    }
  };

  recurse(v, depth);
}
} // namespace nix
