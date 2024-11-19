#include <nix/expr/eval.hh>

namespace nixt {

std::optional<nix::Value> getField(nix::EvalState &State, nix::Value &V,
                                   std::string_view Field);
std::optional<std::string_view>
getFieldString(nix::EvalState &State, nix::Value &V, std::string_view Field);

/// \brief Check if value \p V is an attrset, has the field, and equals to \p
/// Pred
bool checkField(nix::EvalState &State, nix::Value &V, std::string_view Field,
                std::string_view Pred);

/// \brief Check if value is an attrset, and it's "_type" equals to \p Pred
bool checkType(nix::EvalState &State, nix::Value &V, std::string_view Pred);

bool isOption(nix::EvalState &State, nix::Value &V);

bool isDerivation(nix::EvalState &State, nix::Value &V);

std::string attrPathStr(nix::EvalState &State, nix::Value &V,
                        const std::string &AttrPath);

/// \brief Transform a vector of string into a vector of nix symbols.
std::vector<nix::Symbol> toSymbols(nix::SymbolTable &STable,
                                   const std::vector<std::string> &Names);

/// \copydoc toSymbols
std::vector<nix::Symbol> toSymbols(nix::SymbolTable &STable,
                                   const std::vector<std::string_view> &Names);

/// \brief Select attribute \p Attr
nix::Value &selectAttr(nix::EvalState &State, nix::Value &V, nix::Symbol Attr);

nix::Value &selectOption(nix::EvalState &State, nix::Value &V,
                         nix::Symbol Attr);

/// \brief Given an attrpath in nix::Value \p V, select it
nix::Value &selectAttrPath(nix::EvalState &State, nix::Value &V,
                           std::vector<nix::Symbol>::const_iterator Begin,
                           std::vector<nix::Symbol>::const_iterator End);

/// \brief Select the option declaration list, \p V,  dive into "submodules".
nix::Value selectOptions(nix::EvalState &State, nix::Value &V,
                         std::vector<nix::Symbol>::const_iterator Begin,
                         std::vector<nix::Symbol>::const_iterator End);

inline nix::Value selectOptions(nix::EvalState &State, nix::Value &V,
                                const std::vector<nix::Symbol> &AttrPath) {
  return selectOptions(State, V, AttrPath.begin(), AttrPath.end());
}

/// \copydoc selectAttrPath
inline nix::Value &selectSymbols(nix::EvalState &State, nix::Value &V,
                                 const std::vector<nix::Symbol> &AttrPath) {
  return selectAttrPath(State, V, AttrPath.begin(), AttrPath.end());
}

/// \copydoc selectAttrPath
inline nix::Value &selectStrings(nix::EvalState &State, nix::Value &V,
                                 const std::vector<std::string> &AttrPath) {
  return selectSymbols(State, V, toSymbols(State.symbols, AttrPath));
}

/// \copydoc selectAttrPath
inline nix::Value &
selectStringViews(nix::EvalState &State, nix::Value &V,
                  const std::vector<std::string_view> &AttrPath) {
  return selectSymbols(State, V, toSymbols(State.symbols, AttrPath));
}

/// \brief Get nix's `builtins` constant
inline nix::Value &getBuiltins(const nix::EvalState &State) {
  return *State.baseEnv.values[0];
}

} // namespace nixt
