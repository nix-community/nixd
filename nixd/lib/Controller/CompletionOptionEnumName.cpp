#include "CompletionOptionEnum.h"

using namespace nixd;
using namespace lspserver;

void nixd::addOptionEnumNameItems(const OptionField &Field,
                                  const OptionDescription &Desc,
                                  const std::string &ModuleOrigin,
                                  const AddCompletionItem &AddItem) {
  if (!Desc.Type || !Desc.Type->EnumValues)
    return;

  std::string Detail = ModuleOrigin;
  if (Desc.Type->Description)
    Detail += " | " + *Desc.Type->Description;

  for (const std::string &Value : *Desc.Type->EnumValues) {
    AddItem(CompletionItem{
        .label = Field.Name + " = " + Value,
        .kind = CompletionItemKind::EnumMember,
        .detail = Detail,
        .documentation =
            MarkupContent{
                .kind = MarkupKind::Markdown,
                .value = Desc.Description.value_or(""),
            },
        .insertText = Field.Name + " = " + quoteNixString(Value) + ";",
        .insertTextFormat = InsertTextFormat::PlainText,
    });
  }
}
