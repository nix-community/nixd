#pragma once

#include <memory>
#include <type_traits>

namespace nixf {

/// \brief Optionally own a node.
template <class T, class U = T> class UniqueOrRaw {
  T *Node;
  std::unique_ptr<U> OwnedNode;

  static_assert(std::is_base_of_v<T, U> || std::is_same_v<T, U>,
                "U must be derived from T or T itself");

public:
  UniqueOrRaw(T *Node) : Node(Node) {}
  UniqueOrRaw(std::unique_ptr<U> Node)
      : Node(Node.get()), OwnedNode(std::move(Node)) {}
  [[gnu::always_inline]] [[nodiscard]] T *getRaw() const { return Node; }
  [[gnu::always_inline]] [[nodiscard]] U *getUnique() const {
    return OwnedNode.get();
  }
};

} // namespace nixf
