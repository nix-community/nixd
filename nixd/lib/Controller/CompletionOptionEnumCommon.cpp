#include "CompletionOptionEnum.h"

#include <llvm/Support/Error.h>

#include <semaphore>

using namespace nixd;
using namespace lspserver;

namespace {

std::string escapeNixString(llvm::StringRef Origin) {
  std::string Ret;
  for (char Ch : Origin) {
    switch (Ch) {
    case '\\':
      Ret += "\\\\";
      break;
    case '"':
      Ret += "\\\"";
      break;
    case '$':
      Ret += "\\$";
      break;
    case '\n':
      Ret += "\\n";
      break;
    case '\r':
      Ret += "\\r";
      break;
    case '\t':
      Ret += "\\t";
      break;
    default:
      Ret += Ch;
      break;
    }
  }
  return Ret;
}

} // namespace

std::string nixd::quoteNixString(llvm::StringRef Origin) {
  return "\"" + escapeNixString(Origin) + "\"";
}

std::optional<OptionDescription>
nixd::resolveOptionInfo(AttrSetClient &Client,
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

void nixd::completeOptionEnumValuesForScope(
    const std::vector<std::string> &Scope, std::mutex &OptionsLock,
    Controller::OptionMapTy &Options, const AddCompletionItem &AddItem,
    bool InsertQuotedValue, std::optional<Range> Range) {
  std::lock_guard _(OptionsLock);
  for (const auto &[Name, Provider] : Options) {
    AttrSetClient *Client = Provider->client();
    if (!Client) [[unlikely]] {
      elog("skipped client {0} as it is dead", Name);
      continue;
    }

    std::optional<OptionDescription> Desc = resolveOptionInfo(*Client, Scope);
    if (!Desc || !Desc->Type)
      continue;

    const OptionType &Type = *Desc->Type;
    if (Type.Name != "enum" || !Type.EnumValues || Type.EnumValues->empty())
      continue;

    std::string Detail = Name;
    if (Type.Description)
      Detail += " | " + *Type.Description;

    for (const std::string &Value : *Type.EnumValues) {
      CompletionItem Item{
          .label = Value,
          .kind = CompletionItemKind::EnumMember,
          .detail = Detail,
      };
      if (InsertQuotedValue) {
        Item.insertText = quoteNixString(Value);
        Item.insertTextFormat = InsertTextFormat::PlainText;
      } else if (Range) {
        Item.insertTextFormat = InsertTextFormat::PlainText;
        Item.textEdit = TextEdit{
            .range = *Range,
            .newText = " = " + quoteNixString(Value) + ";",
        };
      }
      AddItem(std::move(Item));
    }
  }
}
