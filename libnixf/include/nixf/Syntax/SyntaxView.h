#pragma once

#include "nixf/Basic/Diagnostic.h"
#include "nixf/Syntax/RawSyntax.h"

#include <memory>
#include <utility>
namespace nixf {

class SyntaxView : std::enable_shared_from_this<SyntaxView> {

  std::weak_ptr<SyntaxView> Parent;
  std::shared_ptr<RawSyntax> Raw;

  using ChildTy = std::vector<std::shared_ptr<SyntaxView>>;

  ChildTy Children;

public:
  explicit SyntaxView(std::shared_ptr<RawSyntax> Raw) : Raw(std::move(Raw)) {
    for (const auto &Ch : Raw->Layout) {
      auto ChView = std::make_shared<SyntaxView>(Ch);
      ChView->Parent = weak_from_this();
      Children.emplace_back();
    }
  }
};

} // namespace nixf
