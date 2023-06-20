#include "nixd/nix/Value.h"

#include <nix/eval.hh>
#include <nix/print.hh>

namespace nix::nixd {

bool isOption(EvalState &State, Value &V) {
  State.forceValue(V, noPos);
  if (V.type() != ValueType::nAttrs)
    return false;

  // https://github.com/NixOS/nixpkgs/blob/58ca986543b591a8269cbce3328293ca8d64480f/lib/options.nix#L89
  auto S = attrPathStr(State, V, "_type");
  return S && S.value() == "option";
};

bool isDerivation(EvalState &State, Value &V) {
  State.forceValue(V, noPos);
  if (V.type() != ValueType::nAttrs)
    return false;

  // Derivations has a special attribute "type" == "derivation"
  auto S = attrPathStr(State, V, "type");
  return S && S.value() == "derivation";
}

std::optional<std::string> attrPathStr(nix::EvalState &State, nix::Value &V,
                                       const std::string &AttrPath) noexcept {
  try {
    auto [VPath, Pos] =
        findAlongAttrPath(State, AttrPath, *State.allocBindings(0), V);
    State.forceValue(*VPath, Pos);
    if (VPath->type() == nix::ValueType::nString)
      return std::string(State.forceStringNoCtx(*VPath, nix::noPos, ""));
  } catch (std::exception &E) {
  } catch (...) {
  }
  return std::nullopt;
};

int PrintDepth;

} // namespace nix::nixd

namespace nix {

// Use this strong symbol to override nix's implementation
void Value::print(const SymbolTable &symbols, std::ostream &str,
                  std::set<const void *> *seen) const {
  if (nixd::PrintDepth == 0) {
    str << "{...}";
    return;
  }

  struct DepthRAII {
    decltype(nixd::PrintDepth) &Depth;
    DepthRAII(decltype(Depth) &D) : Depth(D) { Depth--; }
    ~DepthRAII() { Depth++; }
  } _(nixd::PrintDepth);

  // Copy-pasted from upstream:

  switch (internalType) {
  case tInt:
    str << integer;
    break;
  case tBool:
    printLiteralBool(str, boolean);
    break;
  case tString:
    printLiteralString(str, string.s);
    break;
  case tPath:
    str << path().to_string(); // !!! escaping?
    break;
  case tNull:
    str << "null";
    break;
  case tAttrs: {
    if (seen && !attrs->empty() && !seen->insert(attrs).second)
      str << "«repeated»";
    else {
      str << "{ ";
      for (auto &i : attrs->lexicographicOrder(symbols)) {
        str << symbols[i->name] << " = ";
        i->value->print(symbols, str, seen);
        str << "; ";
      }
      str << "}";
    }
    break;
  }
  case tList1:
  case tList2:
  case tListN:
    if (seen && listSize() && !seen->insert(listElems()).second)
      str << "«repeated»";
    else {
      str << "[ ";
      for (auto v2 : listItems()) {
        if (v2)
          v2->print(symbols, str, seen);
        else
          str << "(nullptr)";
        str << " ";
      }
      str << "]";
    }
    break;
  case tThunk:
  case tApp:
    str << "<CODE>";
    break;
  case tLambda:
    str << "<LAMBDA>";
    break;
  case tPrimOp:
    str << "<PRIMOP>";
    break;
  case tPrimOpApp:
    str << "<PRIMOP-APP>";
    break;
  case tExternal:
    str << *external;
    break;
  case tFloat:
    str << fpoint;
    break;
  case tBlackhole:
    // Although we know for sure that it's going to be an infinite recursion
    // when this value is accessed _in the current context_, it's likely
    // that the user will misinterpret a simpler «infinite recursion» output
    // as a definitive statement about the value, while in fact it may be
    // a valid value after `builtins.trace` and perhaps some other steps
    // have completed.
    str << "«potential infinite recursion»";
    break;
  default:
    printError(
        "Nix evaluator internal error: Value::print(): invalid value type %1%",
        internalType);
    abort();
  }
}

} // namespace nix
