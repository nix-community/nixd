#include "AST.h"

#include "nixd/Controller/Controller.h"
#include "nixd/Protocol/AttrSet.h"

#include "lspserver/Logger.h"

#include <nixf/Basic/Nodes/Attrs.h>
#include <nixf/Basic/Nodes/Basic.h>
#include <nixf/Basic/Nodes/Simple.h>

#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>

#include <optional>
#include <semaphore>
#include <sstream>

using namespace lspserver;
using namespace nixd;
using namespace nixf;

namespace {

constexpr llvm::StringLiteral OptionValueTypeCode = "option-value-type";

std::optional<OptionDescription>
resolveOptionInfo(AttrSetClient &Client,
                  const std::vector<std::string> &Scope) {
  std::binary_semaphore Ready(0);
  std::optional<OptionDescription> Desc;
  auto OnReply = [&Ready, &Desc](llvm::Expected<OptionInfoResponse> Resp) {
    if (Resp)
      Desc = *Resp;
    else
      llvm::consumeError(Resp.takeError());
    Ready.release();
  };

  Client.optionInfo(Scope, std::move(OnReply));
  Ready.acquire();
  return Desc;
}

std::string joinOptionPath(const std::vector<std::string> &Scope) {
  std::ostringstream OS;
  for (std::size_t I = 0; I < Scope.size(); ++I) {
    if (I != 0)
      OS << ".";
    OS << Scope[I];
  }
  return OS.str();
}

bool enumContains(const OptionType &Type, llvm::StringRef Value) {
  assert(Type.EnumValues && "must have enum values");
  return llvm::is_contained(*Type.EnumValues, Value);
}

std::string sourceText(llvm::StringRef Src, LexerCursorRange Range) {
  return Src
      .substr(Range.lCur().offset(),
              Range.rCur().offset() - Range.lCur().offset())
      .str();
}

std::optional<std::string> getLiteralString(const ExprString &String) {
  if (String.parts().fragments().empty())
    return "";

  if (!String.isLiteral())
    return std::nullopt;

  return String.literal();
}

std::optional<NixdDiagnostic>
validateOptionBinding(const Binding &Binding, const ParentMapAnalysis &PM,
                      llvm::StringRef Src, Controller::OptionMapTy &Options) {
  const auto &Value = Binding.value();
  if (!Value || Value->kind() != Node::NK_ExprString)
    return std::nullopt;

  const auto &String = static_cast<const ExprString &>(*Value);
  std::optional<std::string> LiteralString = getLiteralString(String);
  if (!LiteralString)
    return std::nullopt;

  const auto &Names = Binding.path().names();
  if (Names.empty())
    return std::nullopt;

  std::vector<std::string> Scope;
  if (findAttrPathForOptions(*Names.back(), PM, Scope) !=
          FindAttrPathResult::OK ||
      Scope.empty())
    return std::nullopt;

  for (const auto &[Name, Provider] : Options) {
    AttrSetClient *Client = Provider->client();
    if (!Client) {
      elog("skipped option client {0} as it is dead", Name);
      continue;
    }

    std::optional<OptionDescription> Desc = resolveOptionInfo(*Client, Scope);
    if (!Desc || !Desc->Type)
      continue;

    const OptionType &Type = *Desc->Type;
    if (Type.Name != "enum")
      continue;

    if (!Type.EnumValues)
      return std::nullopt;

    if (enumContains(Type, *LiteralString))
      return std::nullopt;

    std::string TypeDescription =
        Type.Description.value_or(Type.Name.value_or("expected option type"));
    return NixdDiagnostic{
        .Range = Value->range(),
        .Severity = NixdDiagnosticSeverity::Error,
        .Code = std::string(OptionValueTypeCode),
        .Source = "nixd",
        .Message = "definition `" + sourceText(Src, Value->range()) +
                   "` for option `" + joinOptionPath(Scope) +
                   "` is not of type `" + TypeDescription + "`.",
    };
  }

  return std::nullopt;
}

void collectOptionDiagnosticsFromNode(
    const Node &N, const ParentMapAnalysis &PM, llvm::StringRef Src,
    Controller::OptionMapTy &Options,
    std::vector<NixdDiagnostic> &Diagnostics) {
  if (N.kind() == Node::NK_Binding) {
    if (std::optional<NixdDiagnostic> Diag = validateOptionBinding(
            static_cast<const Binding &>(N), PM, Src, Options))
      Diagnostics.emplace_back(std::move(*Diag));
  }

  for (const Node *Child : N.children()) {
    if (Child)
      collectOptionDiagnosticsFromNode(*Child, PM, Src, Options, Diagnostics);
  }
}

} // namespace

std::vector<NixdDiagnostic>
Controller::collectOptionDiagnostics(const NixTU &TU) {
  std::vector<NixdDiagnostic> Diagnostics;
  if (!TU.ast() || !TU.parentMap())
    return Diagnostics;

  std::lock_guard _(OptionsLock);
  collectOptionDiagnosticsFromNode(*TU.ast(), *TU.parentMap(), TU.src(),
                                   Options, Diagnostics);
  return Diagnostics;
}
