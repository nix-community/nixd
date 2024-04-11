#include "nixt/Value.h"

#include <nix/attr-path.hh>
#include <nix/nixexpr.hh>
#include <nix/value.hh>

using namespace nixt;

bool nixt::isOption(nix::EvalState &State, nix::Value &V) {
  State.forceValue(V, nix::noPos);
  if (V.type() != nix::ValueType::nAttrs)
    return false;

  // https://github.com/NixOS/nixpkgs/blob/58ca986543b591a8269cbce3328293ca8d64480f/lib/options.nix#L89
  try {
    auto S = attrPathStr(State, V, "_type");
    return S == "option";
  } catch (nix::AttrPathNotFound &Error) {
    return false;
  }
};

bool nixt::isDerivation(nix::EvalState &State, nix::Value &V) {
  State.forceValue(V, nix::noPos);
  if (V.type() != nix::ValueType::nAttrs)
    return false;

  try {
    // Derivations has a special attribute "type" == "derivation"
    auto S = attrPathStr(State, V, "type");
    return S == "derivation";
  } catch (nix::AttrPathNotFound &Error) {
    return false;
  }
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
    throw nix::TypeError("value is not an attrset");

  assert(V.attrs && "nix must allocate non-null attrs!");
  auto *Nested = V.attrs->find(Attr);
  if (Nested == V.attrs->end())
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
