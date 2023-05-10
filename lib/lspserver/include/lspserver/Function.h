#pragma once

#include <llvm/ADT/FunctionExtras.h>
#include <llvm/Support/Error.h>
#include <mutex>
#include <tuple>
#include <utility>

namespace lspserver {

/// A Callback<T> is a void function that accepts Expected<T>.
/// This is accepted by ClangdServer functions that logically return T.
template <typename T>
using Callback = llvm::unique_function<void(llvm::Expected<T>)>;

/// An Event<T> allows events of type T to be broadcast to listeners.
template <typename T> class Event {
public:
  // A Listener is the callback through which events are delivered.
  using Listener = std::function<void(const T &)>;

  // A subscription defines the scope of when a listener should receive events.
  // After destroying the subscription, no more events are received.
  class [[nodiscard]] Subscription {
    Event *Parent;
    unsigned ListenerID;

    Subscription(Event *Parent, unsigned ListenerID)
        : Parent(Parent), ListenerID(ListenerID) {}
    friend Event;

  public:
    Subscription() : Parent(nullptr) {}
    Subscription(Subscription &&Other) : Parent(nullptr) {
      *this = std::move(Other);
    }
    Subscription &operator=(Subscription &&Other) {
      // If *this is active, unsubscribe.
      if (Parent) {
        std::lock_guard<std::recursive_mutex> Lock(Parent->ListenersMu);
        llvm::erase_if(Parent->Listeners,
                       [&](const std::pair<Listener, unsigned> &P) {
                         return P.second == ListenerID;
                       });
      }
      // Take over the other subscription, and mark it inactive.
      std::tie(Parent, ListenerID) = std::tie(Other.Parent, Other.ListenerID);
      Other.Parent = nullptr;
      return *this;
    }
    // Destroying a subscription may block if an event is being broadcast.
    ~Subscription() {
      if (Parent)
        *this = Subscription(); // Unsubscribe.
    }
  };

  // Adds a listener that will observe all future events until the returned
  // subscription is destroyed.
  // May block if an event is currently being broadcast.
  Subscription observe(Listener L) {
    std::lock_guard<std::recursive_mutex> Lock(ListenersMu);
    Listeners.push_back({std::move(L), ++ListenerCount});
    return Subscription(this, ListenerCount);
  }

  // Synchronously sends an event to all registered listeners.
  // Must not be called from a listener to this event.
  void broadcast(const T &V) {
    // FIXME: it would be nice to dynamically check non-reentrancy here.
    std::lock_guard<std::recursive_mutex> Lock(ListenersMu);
    for (const auto &L : Listeners)
      L.first(V);
  }

  ~Event() {
    std::lock_guard<std::recursive_mutex> Lock(ListenersMu);
    assert(Listeners.empty());
  }

private:
  static_assert(std::is_same<std::decay_t<T>, T>::value,
                "use a plain type: event values are always passed by const&");

  std::recursive_mutex ListenersMu;
  bool IsBroadcasting = false;
  std::vector<std::pair<Listener, unsigned>> Listeners;
  unsigned ListenerCount = 0;
};

} // namespace lspserver
