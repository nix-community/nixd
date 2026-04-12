#include "CompletionOptionEnum.h"

#include <llvm/ADT/SmallVector.h>

#include <algorithm>
#include <cctype>

using namespace nixd;

namespace {

bool isPathChar(char Ch) {
  return std::isalnum(static_cast<unsigned char>(Ch)) || Ch == '_' ||
         Ch == '-' || Ch == '\'' || Ch == '.';
}

std::optional<std::vector<std::string>>
findOptionScopeBeforeMissingValue(llvm::StringRef Src, std::size_t Offset) {
  Offset = std::min(Offset, Src.size());
  llvm::StringRef BeforeCursor = Src.take_front(Offset);
  std::size_t Eq = BeforeCursor.rfind('=');
  if (Eq == llvm::StringRef::npos)
    return std::nullopt;

  if (!BeforeCursor.drop_front(Eq + 1).trim().empty())
    return std::nullopt;

  llvm::StringRef BeforeEq = BeforeCursor.take_front(Eq).rtrim();
  std::size_t End = BeforeEq.size();
  std::size_t Begin = End;
  while (Begin > 0 && isPathChar(BeforeEq[Begin - 1]))
    --Begin;

  llvm::StringRef Path = BeforeEq.slice(Begin, End).trim();
  if (Path.empty())
    return std::nullopt;

  std::vector<std::string> Scope;
  llvm::SmallVector<llvm::StringRef, 8> Parts;
  Path.split(Parts, '.');
  Scope.reserve(Parts.size());
  for (llvm::StringRef Part : Parts) {
    Part = Part.trim();
    if (Part.empty())
      return std::nullopt;
    Scope.emplace_back(Part.str());
  }

  if (!Scope.empty() && Scope.front() == "config")
    Scope.erase(Scope.begin());
  if (Scope.empty())
    return std::nullopt;
  return Scope;
}

} // namespace

void nixd::completeOptionEnumValuesAfterEq(llvm::StringRef Src,
                                           std::size_t Offset,
                                           std::mutex &OptionsLock,
                                           Controller::OptionMapTy &Options,
                                           const AddCompletionItem &AddItem) {
  std::optional<std::vector<std::string>> Scope =
      findOptionScopeBeforeMissingValue(Src, Offset);
  if (!Scope)
    return;

  completeOptionEnumValuesForScope(*Scope, OptionsLock, Options, AddItem,
                                   /*InsertQuotedValue=*/true);
}
