#pragma once

#include <memory>
#include <vector>

namespace nixd {
template <class T> class GCPool {
public:
  std::vector<std::unique_ptr<T>> Nodes;
  template <class U> U *addNode(std::unique_ptr<U> Node) {
    Nodes.push_back(std::move(Node));
    return dynamic_cast<U *>(Nodes.back().get());
  }
  template <class U> U *record(U *Node) {
    Nodes.emplace_back(std::unique_ptr<U>(Node));
    return Node;
  }
};
} // namespace nixd
