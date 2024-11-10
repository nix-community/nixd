#include "nixt/Value.h"

#include <nix/attr-path.hh>
#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>
#include <nix/value.hh>

using namespace nixt;

std::optional<nix::Value> nixt::getField(nix::EvalState &State, nix::Value &V,
                                         std::string_view Field) {
  State.forceValue(V, nix::noPos);
  if (V.type() != nix::ValueType::nAttrs)
    return std::nullopt;

  nix::Symbol SFiled = State.symbols.create(Field);
  if (auto *It = V.attrs()->find(SFiled); It != V.attrs()->end())
    return *It->value;

  return std::nullopt;
}

std::optional<std::string_view> nixt::getFieldString(nix::EvalState &State,
                                                     nix::Value &V,
                                                     std::string_view Field) {
  if (auto OptV = getField(State, V, Field)) {
    State.forceValue(*OptV, nix::noPos);
    if (OptV->type() == nix::ValueType::nString) {
      return State.forceStringNoCtx(*OptV, nix::noPos,
                                    "nixt::getFieldString()");
    }
  }
  return std::nullopt;
}

bool nixt::checkField(nix::EvalState &State, nix::Value &V,
                      std::string_view Field, std::string_view Pred) {
  return getFieldString(State, V, Field) == Pred;
}

bool nixt::checkType(nix::EvalState &State, nix::Value &V,
                     std::string_view Pred) {
  return checkField(State, V, "_type", Pred);
}

bool nixt::isOption(nix::EvalState &State, nix::Value &V) {
  return checkType(State, V, "option");
};

bool nixt::isDerivation(nix::EvalState &State, nix::Value &V) {
  return checkField(State, V, "type", "derivation");
}

std::string nixt::attrPathStr(nix::EvalState &State, nix::Value &V,
                              const std::string &AttrPath) {
  auto &AutoArgs = *State.allocBindings(0);
  auto [VPath, Pos] = nix::findAlongAttrPath(State, AttrPath, AutoArgs, V);
  State.forceValue(*VPath, Pos);
  return std::string(State.forceStringNoCtx(*VPath, nix::noPos, ""));
}

std::vector<nix::Symbol>
nixt::toSymbols(nix::SymbolTable &STable,
                const std::vector<std::string> &Names) {
  std::vector<nix::Symbol> Res;
  Res.reserve(Names.size());
  for (const auto &Name : Names) {
    Res.emplace_back(STable.create(Name));
  }
  return Res;
}

std::vector<nix::Symbol>
nixt::toSymbols(nix::SymbolTable &STable,
                const std::vector<std::string_view> &Names) {
  std::vector<nix::Symbol> Res;
  Res.reserve(Names.size());
  for (const auto &Name : Names) {
    Res.emplace_back(STable.create(Name));
  }
  return Res;
}

nix::Value &nixt::selectAttr(nix::EvalState &State, nix::Value &V,
                             nix::Symbol Attr) {
  State.forceValue(V, nix::noPos);

  if (V.type() != nix::ValueType::nAttrs)
    throw nix::TypeError(State, "value is not an attrset");

  assert(V.attrs() && "nix must allocate non-null attrs!");
  auto *Nested = V.attrs()->find(Attr);
  if (Nested == V.attrs()->end())
    throw nix::AttrPathNotFound("attrname " + State.symbols[Attr] +
                                " not found in attrset");

  assert(Nested->value && "nix must allocate non-null nested value!");
  return *Nested->value;
}

/// \brief Given an attrpath in nix::Value \p V, select it
nix::Value &nixt::selectAttrPath(nix::EvalState &State, nix::Value &V,
                                 std::vector<nix::Symbol>::const_iterator Begin,
                                 std::vector<nix::Symbol>::const_iterator End) {
  // If the attrpath is emtpy, return value itself.
  if (Begin == End)
    return V;

  // Otherwise, select it.
  nix::Value &Nested = selectAttr(State, V, *Begin);
  return selectAttrPath(State, Nested, ++Begin, End);
}

namespace {

/// \brief Check if the \p Type is a submodule.
bool isTypeSubmodule(nix::EvalState &State, nix::Value &Type) {
  return checkField(State, Type, "name", "submodule");
}

nix::Value *trySelectAttr(nix::EvalState &State, nix::Value &V, nix::Symbol S) {
  try {
    nix::Value &Type = selectAttr(State, V, State.sType);
    return &Type;
  } catch (nix::TypeError &) {
    // The value is not an attrset, thus it definitely cannot be a submodule.
    return nullptr;
  } catch (nix::AttrPathNotFound &) {
    // The value has no "type" field.
    return nullptr;
  }
  return nullptr;
}

/// \brief Get the type of an option.
nix::Value *tryGetSubmoduleType(nix::EvalState &State, nix::Value &V) {
  if (nix::Value *Type = trySelectAttr(State, V, State.sType))
    return isTypeSubmodule(State, *Type) ? Type : nullptr;
  return nullptr;
}

/// \brief Do proper operations to get options declaration on submodule type.
nix::Value getSubOptions(nix::EvalState &State, nix::Value &Type) {
  // For example, programs.nixvim has all options nested into this attrpath.
  nix::Value &GetSubOptions =
      selectAttr(State, Type, State.symbols.create("getSubOptions"));

  auto list = State.buildList(0);
  auto EmptyList = State.allocValue();
  EmptyList->mkList(list);
  // Invoke "GetSubOptions"
  nix::Value VResult;
  State.callFunction(GetSubOptions, *EmptyList, VResult, nix::noPos);
  return VResult;
}

} // namespace

nix::Value nixt::selectOptions(nix::EvalState &State, nix::Value &V,
                               std::vector<nix::Symbol>::const_iterator Begin,
                               std::vector<nix::Symbol>::const_iterator End) {
  // Always try to mangle the value if it is a submodule
  if (nix::Value *SubType = tryGetSubmoduleType(State, V))
    // Invoke getSubOptions on that type, and reset the value to it.
    V = getSubOptions(State, *SubType);

  if (Begin == End)
    return V;

  if (isOption(State, V)) {
    // If currently "V" is an option, it can still be submodules.
    //
    // e.g. users.users <-- the main option stops at here.
    //      networking.interfaces
    //
    // Take care of such case.
    nix::Value &Type = selectAttr(State, V, State.sType);
    if (checkField(State, Type, "name", "attrsOf")) {
      nix::Value NestedTypes =
          selectAttr(State, Type, State.symbols.create("nestedTypes"));
      nix::Value ElemType =
          selectAttr(State, NestedTypes, State.symbols.create("elemType"));

      if (isTypeSubmodule(State, ElemType)) {
        nix::Value ElemOptions = getSubOptions(State, ElemType);
        return selectOptions(State, ElemOptions, ++Begin, End);
      }
    }
  }

  // Otherwise, simply select it.
  nix::Value &Nested = selectAttr(State, V, *Begin);
  return selectOptions(State, Nested, ++Begin, End);
}
