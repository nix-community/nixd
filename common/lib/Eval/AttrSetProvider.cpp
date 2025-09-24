#include "nixd/Eval/AttrSetProvider.h"
#include "nixd/Protocol/AttrSet.h"

#include "lspserver/Protocol.h"

#include <nix/cmd/common-eval-args.hh>
#include <nix/expr/attr-path.hh>
#include <nix/expr/nixexpr.hh>
#include <nix/store/store-open.hh>
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
      Field = Select.string_view();
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
      .line = static_cast<int64_t>(NixPos.line - 1),
      .character = static_cast<int64_t>(NixPos.column - 1),
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

  State.forceValue(File, nix::noPos);
  State.forceValue(Line, nix::noPos);
  State.forceValue(Column, nix::noPos);

  if (File.type() == nix::ValueType::nString)
    Loc.uri = URIForFile::canonicalize(File.c_str(), File.c_str());

  if (Line.type() == nix::ValueType::nInt &&
      Column.type() == nix::ValueType::nInt) {

    // Nix position starts from "1" however lsp starts from zero.
    lspserver::Position Pos = {static_cast<int64_t>(Line.integer()) - 1,
                               static_cast<int64_t>(Column.integer()) - 1};
    Loc.range = {Pos, Pos};
  }
}

void fillOptionDeclarationPositions(nix::EvalState &State, nix::Value &V,
                                    OptionDescription &R) {
  State.forceValue(V, nix::noPos);
  if (V.type() != nix::ValueType::nList)
    return;
  for (nix::Value *Item : V.listView()) {
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
    assert(V.attrs());
    if (auto *It = V.attrs()->find(State.symbols.create("type"));
        It != V.attrs()->end()) [[likely]] {
      OptionType Type;
      fillOptionType(State, *It->value, Type);
      R.Type = std::move(Type);
    }

    if (auto *It = V.attrs()->find(State.symbols.create("example"));
        It != V.attrs()->end()) {
      State.forceValue(*It->value, It->pos);

      // In nixpkgs some examples are nested in "literalExpression"
      if (nixt::checkField(State, *It->value, "_type", "literalExpression")) {
        R.Example = nixt::getFieldString(State, *It->value, "text");
      } else {
        std::ostringstream OS;
        It->value->print(State, OS);
        R.Example = OS.str();
      }
    }
  }
}

std::vector<std::string> completeNames(nix::Value &Scope,
                                       const nix::EvalState &State,
                                       std::string_view Prefix, int Limit) {
  std::vector<std::string> Names;
  const bool Unlimited = Limit <= 0;

  // FIXME: we may want to use "Trie" to speedup the string searching.
  // However as my (roughtly) profiling the critical in this loop is
  // evaluating package details.
  // "Trie"s may not beneficial because it cannot speedup eval.
  for (const auto *AttrPtr : Scope.attrs()->lexicographicOrder(State.symbols)) {
    const nix::Attr &Attr = *AttrPtr;
    const std::string_view Name = State.symbols[Attr.name];
    if (Name.starts_with(Prefix)) {
      Names.emplace_back(Name);
      // We set this a very limited number as to speedup
      if (!Unlimited && Names.size() >= static_cast<std::size_t>(Limit))
        break;
    }
  }
  return Names;
}

std::optional<ValueDescription> describeValue(nix::EvalState &State,
                                              nix::Value &V) {
  if (V.isPrimOp()) {
    const auto *PrimOp = V.primOp();
    assert(PrimOp);
    return ValueDescription{
        .Doc = PrimOp->doc ? std::string(PrimOp->doc) : "",
        .Arity = static_cast<int>(PrimOp->arity),
        .Args = PrimOp->args,
    };
  } else if (V.isLambda()) {
    auto *Lambda = V.lambda().fun;
    assert(Lambda);
    const auto DocComment = Lambda->docComment;

    // We can only get the comment in the function, not the Arity and Args
    // information. Therefore, if the comment doesn't exist, return
    // `std::nullopt`, indicating that we didn't get any valuable information.
    // https://github.com/NixOS/nix/blob/ee59af99f8619e17db4289843da62a24302d20b7/src/libexpr/eval.cc#L638
    if (!DocComment)
      return std::nullopt;

    return ValueDescription{
        .Doc = DocComment.getInnerText(State.positions),
        .Arity = 0,
        .Args = {},
    };
  }

  return std::nullopt;
}

} // namespace

AttrSetProvider::AttrSetProvider(std::unique_ptr<InboundPort> In,
                                 std::unique_ptr<OutboundPort> Out)
    : LSPServer(std::move(In), std::move(Out)),
      State(new nix::EvalState({}, nix::openStore(), nix::fetchSettings,
                               nix::evalSettings)) {
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
    nix::Expr *AST = state().parseExprFromString(Name, state().rootPath("."));
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
          .ValueDesc = describeValue(state(), V),
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

    const int Limit = Params.MaxItems.value_or(MaxItems);
    return Reply(completeNames(Scope, state(), Params.Prefix, Limit));
  } catch (const nix::BaseError &Err) {
    return Reply(error(Err.info().msg.str()));
  } catch (const std::exception &Err) {
    return Reply(error(Err.what()));
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
         Scope.attrs()->lexicographicOrder(state().symbols)) {
      const nix::Attr &Attr = *AttrPtr;
      std::string_view Name = state().symbols[Attr.name];
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
