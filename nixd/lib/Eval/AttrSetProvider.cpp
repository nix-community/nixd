#include "nixd/Eval/AttrSetProvider.h"
#include "nixd/Protocol/AttrSet.h"

#include "lspserver/Protocol.h"

#include <nix/attr-path.hh>
#include <nix/nixexpr.hh>
#include <nix/store-api.hh>
#include <nixt/Value.h>

using namespace nixd;
using namespace lspserver;

namespace {

constexpr int MaxItems = 30;

void fillString(nix::EvalState &State, nix::Value &V,
                const std::vector<std::string_view> &AttrPath,
                std::optional<std::string> &Field) {
  try {
    nix::Value &Select = nixt::selectStringViews(State, V, AttrPath);
    State.forceValue(Select, nix::noPos);
    if (Select.type() == nix::ValueType::nString)
      Field = Select.string.c_str;
  } catch (std::exception &E) {
    Field = std::nullopt;
  }
}

/// Describe the value as if \p Package is actually a nixpkgs package.
PackageDescription describePackage(nix::EvalState &State, nix::Value &Package) {
  PackageDescription R;
  fillString(State, Package, {"name"}, R.Name);
  fillString(State, Package, {"pname"}, R.PName);
  fillString(State, Package, {"version"}, R.Version);
  fillString(State, Package, {"meta", "description"}, R.Description);
  fillString(State, Package, {"meta", "longDescription"}, R.LongDescription);
  fillString(State, Package, {"meta", "position"}, R.Position);
  fillString(State, Package, {"meta", "homepage"}, R.Homepage);
  return R;
}

std::optional<Location> locationOf(nix::PosTable &PTable, nix::Value &V) {
  nix::PosIdx P = V.determinePos(nix::noPos);
  if (!P)
    return std::nullopt;

  nix::Pos NixPos = PTable[P];
  const auto *SP = std::get_if<nix::SourcePath>(&NixPos.origin);

  if (!SP)
    return std::nullopt;

  Position LPos = {
      .line = static_cast<int>(NixPos.line - 1),
      .character = static_cast<int>(NixPos.column - 1),
  };

  return Location{
      .uri = URIForFile::canonicalize(SP->path.abs(), SP->path.abs()),
      .range = {LPos, LPos},
  };
}

ValueMeta metadataOf(nix::EvalState &State, nix::Value &V) {
  return {
      .Type = V.type(true),
      .Location = locationOf(State.positions, V),
  };
}

void fillUnsafeGetAttrPosLocation(nix::EvalState &State, nix::Value &V,
                                  lspserver::Location &Loc) {
  State.forceValue(V, nix::noPos);
  nix::Value &File = nixt::selectAttr(State, V, State.symbols.create("file"));
  nix::Value &Line = nixt::selectAttr(State, V, State.symbols.create("line"));
  nix::Value &Column =
      nixt::selectAttr(State, V, State.symbols.create("column"));

  if (File.type() == nix::ValueType::nString)
    Loc.uri = URIForFile::canonicalize(File.c_str(), File.c_str());

  if (Line.type() == nix::ValueType::nInt &&
      Column.type() == nix::ValueType::nInt) {

    // Nix position starts from "1" however lsp starts from zero.
    lspserver::Position Pos = {static_cast<int>(Line.integer) - 1,
                               static_cast<int>(Column.integer) - 1};
    Loc.range = {Pos, Pos};
  }
}

void fillOptionDeclarationPositions(nix::EvalState &State, nix::Value &V,
                                    OptionDescription &R) {
  State.forceValue(V, nix::noPos);
  if (V.type() != nix::ValueType::nList)
    return;
  for (nix::Value *Item : V.listItems()) {
    // Each item should have "column", "line", "file" fields.
    lspserver::Location Loc;
    fillUnsafeGetAttrPosLocation(State, *Item, Loc);
    R.Declarations.emplace_back(std::move(Loc));
  }
}

void fillOptionDeclarations(nix::EvalState &State, nix::Value &V,
                            OptionDescription &R) {
  // Eval declarations
  try {
    nix::Value &DeclarationPositions = nixt::selectAttr(
        State, V, State.symbols.create("declarationPositions"));

    State.forceValue(DeclarationPositions, nix::noPos);
    // A list of positions, in unsafeGetAttrPos format.
    fillOptionDeclarationPositions(State, DeclarationPositions, R);
  } catch (nix::AttrPathNotFound &E) {
    // FIXME: fallback to "declarations"
    return;
  }
}

void fillOptionType(nix::EvalState &State, nix::Value &VType, OptionType &R) {
  fillString(State, VType, {"description"}, R.Description);
  fillString(State, VType, {"name"}, R.Name);
}

void fillOptionDescription(nix::EvalState &State, nix::Value &V,
                           OptionDescription &R) {
  fillString(State, V, {"description"}, R.Description);
  fillOptionDeclarations(State, V, R);
  // FIXME: add definitions location.
  if (V.type() == nix::ValueType::nAttrs) [[likely]] {
    assert(V.attrs);
    if (auto *It = V.attrs->find(State.symbols.create("type"));
        It != V.attrs->end()) [[likely]] {
      OptionType Type;
      fillOptionType(State, *It->value, Type);
      R.Type = std::move(Type);
    }

    if (auto *It = V.attrs->find(State.symbols.create("example"));
        It != V.attrs->end()) {
      State.forceValue(*It->value, It->pos);

      // In nixpkgs some examples are nested in "literalExpression"
      if (nixt::checkField(State, *It->value, "_type", "literalExpression")) {
        R.Example = nixt::getFieldString(State, *It->value, "text");
      } else {
        std::ostringstream OS;
        It->value->print(State.symbols, OS);
        R.Example = OS.str();
      }
    }
  }
}

} // namespace

AttrSetProvider::AttrSetProvider(std::unique_ptr<InboundPort> In,
                                 std::unique_ptr<OutboundPort> Out)
    : LSPServer(std::move(In), std::move(Out)),
      State(new nix::EvalState({}, nix::openStore())) {
  Registry.addMethod(rpcMethod::EvalExpr, this, &AttrSetProvider::onEvalExpr);
  Registry.addMethod(rpcMethod::AttrPathInfo, this,
                     &AttrSetProvider::onAttrPathInfo);
  Registry.addMethod(rpcMethod::AttrPathComplete, this,
                     &AttrSetProvider::onAttrPathComplete);
  Registry.addMethod(rpcMethod::OptionInfo, this,
                     &AttrSetProvider::onOptionInfo);
  Registry.addMethod(rpcMethod::OptionComplete, this,
                     &AttrSetProvider::onOptionComplete);
}

void AttrSetProvider::onEvalExpr(
    const std::string &Name,
    lspserver::Callback<std::optional<std::string>> Reply) {
  try {
    nix::Expr *AST = state().parseExprFromString(
        Name, state().rootPath(nix::CanonPath::fromCwd()));
    state().eval(AST, Nixpkgs);
    Reply(std::nullopt);
    return;
  } catch (const nix::BaseError &Err) {
    Reply(error(Err.info().msg.str()));
    return;
  } catch (const std::exception &Err) {
    Reply(error(Err.what()));
    return;
  }
}

void AttrSetProvider::onAttrPathInfo(
    const AttrPathInfoParams &AttrPath,
    lspserver::Callback<AttrPathInfoResponse> Reply) {
  using RespT = AttrPathInfoResponse;
  Reply([&]() -> llvm::Expected<RespT> {
    try {
      if (AttrPath.empty())
        return error("attrpath is empty!");

      nix::Value &V = nixt::selectStrings(state(), Nixpkgs, AttrPath);
      state().forceValue(V, nix::noPos);
      return RespT{
          .Meta = metadataOf(state(), V),
          .PackageDesc = describePackage(state(), V),
      };
    } catch (const nix::BaseError &Err) {
      return error(Err.info().msg.str());
    } catch (const std::exception &Err) {
      return error(Err.what());
    }
  }());
}

void AttrSetProvider::onAttrPathComplete(
    const AttrPathCompleteParams &Params,
    lspserver::Callback<AttrPathCompleteResponse> Reply) {
  try {
    nix::Value &Scope = nixt::selectStrings(state(), Nixpkgs, Params.Scope);

    state().forceValue(Scope, nix::noPos);

    if (Scope.type() != nix::ValueType::nAttrs) {
      Reply(error("scope is not an attrset"));
      return;
    }

    std::vector<std::string> Names;
    int Num = 0;

    // FIXME: we may want to use "Trie" to speedup the string searching.
    // However as my (roughtly) profiling the critical in this loop is
    // evaluating package details.
    // "Trie"s may not beneficial becausae it cannot speedup eval.
    for (const auto *AttrPtr :
         Scope.attrs->lexicographicOrder(state().symbols)) {
      const nix::Attr &Attr = *AttrPtr;
      const std::string Name = state().symbols[Attr.name];
      if (Name.starts_with(Params.Prefix)) {
        ++Num;
        Names.emplace_back(Name);
        // We set this a very limited number as to speedup
        if (Num > MaxItems)
          break;
      }
    }
    Reply(std::move(Names));
    return;
  } catch (const nix::BaseError &Err) {
    Reply(error(Err.info().msg.str()));
    return;
  } catch (const std::exception &Err) {
    Reply(error(Err.what()));
    return;
  }
}

void AttrSetProvider::onOptionInfo(
    const AttrPathInfoParams &AttrPath,
    lspserver::Callback<OptionInfoResponse> Reply) {
  try {
    if (AttrPath.empty()) {
      Reply(error("attrpath is empty!"));
      return;
    }

    nix::Value Option = nixt::selectOptions(
        state(), Nixpkgs, nixt::toSymbols(state().symbols, AttrPath));

    OptionInfoResponse R;

    fillOptionDescription(state(), Option, R);

    Reply(std::move(R));
    return;
  } catch (const nix::BaseError &Err) {
    Reply(error(Err.info().msg.str()));
    return;
  } catch (const std::exception &Err) {
    Reply(error(Err.what()));
    return;
  }
}

void AttrSetProvider::onOptionComplete(
    const AttrPathCompleteParams &Params,
    lspserver::Callback<OptionCompleteResponse> Reply) {
  try {
    nix::Value Scope = nixt::selectOptions(
        state(), Nixpkgs, nixt::toSymbols(state().symbols, Params.Scope));

    state().forceValue(Scope, nix::noPos);

    if (Scope.type() != nix::ValueType::nAttrs) {
      Reply(error("scope is not an attrset"));
      return;
    }

    if (nixt::isOption(state(), Scope)) {
      Reply(error("scope is already an option"));
      return;
    }

    std::vector<OptionField> Response;

    // FIXME: we may want to use "Trie" to speedup the string searching.
    // However as my (roughtly) profiling the critical in this loop is
    // evaluating package details.
    // "Trie"s may not beneficial becausae it cannot speedup eval.
    for (const auto *AttrPtr :
         Scope.attrs->lexicographicOrder(state().symbols)) {
      const nix::Attr &Attr = *AttrPtr;
      std::string Name = state().symbols[Attr.name];
      if (Name.starts_with(Params.Prefix)) {
        // Add a new "OptionField", see it's type.
        assert(Attr.value);
        OptionField NewField;
        NewField.Name = Name;
        if (nixt::isOption(state(), *Attr.value)) {
          OptionDescription Desc;
          fillOptionDescription(state(), *Attr.value, Desc);
          NewField.Description = std::move(Desc);
        }
        Response.emplace_back(std::move(NewField));
        // We set this a very limited number as to speedup
        if (Response.size() >= MaxItems)
          break;
      }
    }
    Reply(std::move(Response));
    return;
  } catch (const nix::BaseError &Err) {
    Reply(error(Err.info().msg.str()));
    return;
  } catch (const std::exception &Err) {
    Reply(error(Err.what()));
    return;
  }
}
