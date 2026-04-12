#include "AST.h"
#include "CompletionOptionEnum.h"

using namespace nixd;
using namespace nixf;

namespace {

const Binding *findStringValueBinding(const Node &N,
                                      const ParentMapAnalysis &PM) {
  const Node *StringNode = PM.upTo(N, Node::NK_ExprString);
  if (!StringNode)
    return nullptr;

  const Node *BindingNode = PM.upTo(*StringNode, Node::NK_Binding);
  if (!BindingNode)
    return nullptr;

  const auto &OptionBinding = static_cast<const Binding &>(*BindingNode);
  if (OptionBinding.value().get() != StringNode)
    return nullptr;

  const auto &String = static_cast<const ExprString &>(*StringNode);
  if (!String.parts().fragments().empty() && !String.isLiteral())
    return nullptr;

  return &OptionBinding;
}

} // namespace

void nixd::completeOptionEnumValuesInString(const Node &N,
                                            const ParentMapAnalysis &PM,
                                            std::mutex &OptionsLock,
                                            Controller::OptionMapTy &Options,
                                            const AddCompletionItem &AddItem) {
  const Binding *OptionBinding = findStringValueBinding(N, PM);
  if (!OptionBinding)
    return;

  const auto &Names = OptionBinding->path().names();
  if (Names.empty())
    return;

  std::vector<std::string> Scope;
  if (findAttrPathForOptions(*Names.back(), PM, Scope) !=
          FindAttrPathResult::OK ||
      Scope.empty())
    return;

  completeOptionEnumValuesForScope(Scope, OptionsLock, Options, AddItem,
                                   /*InsertQuotedValue=*/false);
}
