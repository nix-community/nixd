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

Symbol getName(const AttrName &Name, EvalState &State, Env &E) {
  if (Name.symbol) {
    return Name.symbol;
  }
  Value NameValue;
  Name.expr->eval(State, E, NameValue);
  State.forceStringNoCtx(NameValue, noPos,
                         "while evaluating an attribute name");
  return State.symbols.create(NameValue.string.s);
}

Value evalAttrPath(EvalState &State, Value Base, Env &E, const AttrPath &Path) {
  Value *VAttrs = &Base;

  for (const auto &AttrName : Path) {
    Bindings::iterator It;
    auto Sym = getName(AttrName, State, E);
    State.forceAttrs(*VAttrs, noPos, "while selecting an attribute");
    if ((It = VAttrs->attrs->find(Sym)) == VAttrs->attrs->end()) {
      State.error("attribute '%1%' missing", State.symbols[Sym])
          .atPos(noPos)
          .debugThrow<EvalError>();
    }
    VAttrs = It->value;
  }

  State.forceValue(*VAttrs, noPos);
  return Base;
}

} // namespace nix
