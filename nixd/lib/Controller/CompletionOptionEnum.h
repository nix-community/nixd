#pragma once

#include "lspserver/Protocol.h"

#include "nixd/Controller/Controller.h"
#include "nixd/Protocol/AttrSet.h"

#include <llvm/ADT/StringRef.h>

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace nixf {
class Node;
class ParentMapAnalysis;
} // namespace nixf

namespace nixd {

using AddCompletionItem = std::function<void(lspserver::CompletionItem)>;

std::string quoteNixString(llvm::StringRef Origin);

std::optional<OptionDescription>
resolveOptionInfo(AttrSetClient &Client, const std::vector<std::string> &Scope);

void completeOptionEnumValuesForScope(
    const std::vector<std::string> &Scope, std::mutex &OptionsLock,
    Controller::OptionMapTy &Options, const AddCompletionItem &AddItem,
    bool InsertQuotedValue, std::optional<lspserver::Range> Range = {});

void addOptionEnumNameItems(const OptionField &Field,
                            const OptionDescription &Desc,
                            const std::string &ModuleOrigin,
                            const AddCompletionItem &AddItem);

void completeOptionEnumValuesInString(const nixf::Node &N,
                                      const nixf::ParentMapAnalysis &PM,
                                      std::mutex &OptionsLock,
                                      Controller::OptionMapTy &Options,
                                      const AddCompletionItem &AddItem);

void completeOptionEnumValuesAfterEq(llvm::StringRef Src, std::size_t Offset,
                                     std::mutex &OptionsLock,
                                     Controller::OptionMapTy &Options,
                                     const AddCompletionItem &AddItem);

} // namespace nixd
