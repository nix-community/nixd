/// \file
/// \brief Pointer pool, for RAII memory management.

// TODO: This file is trivial and shared among many libraries, maybe should move
// this in a standalone public header.

#pragma once

#include <memory>
#include <vector>

namespace nixt {

/// \brief A simple pointer pool, a vector of `unique_ptr`s.
///
/// It is used for "owning" nodes. Other classes can use weak/raw pointers to
/// the nodes, to  avoid cyclic references.
///
/// Also in nix AST, the nodes are not owned by it's parent because in bison
/// algorithm nodes should be copyable while performing shift-reduce. So in our
/// implementation nodes are owned in this structure.
template <class T> struct PtrPool {
  std::vector<std::unique_ptr<T>> Nodes;

  /// \brief Takes ownership of a node, add it to the pool.
  template <class U> U *add(std::unique_ptr<U> Node) {
    Nodes.push_back(std::move(Node));
    return dynamic_cast<U *>(Nodes.back().get());
  }

  /// \brief Takes ownership from a raw pointer.
  ///
  /// \note This should only be used when it is allocated by "malloc", and not
  /// owned by other objects (otherwise it will cause double free).
  template <class U> U *record(U *Node) {
    Nodes.emplace_back(std::unique_ptr<U>(Node));
    return Node;
  }
};

} // namespace nixt
