#include <llvm/ADT/FunctionExtras.h>

#include <boost/range/adaptor/reversed.hpp>

template <class T, class U = T>
auto latestMatchOr(const std::vector<T> &Container,
                   llvm::unique_function<bool(T)> Metric, U Or = U{}) -> U {
  for (const auto &R : boost::adaptors::reverse(Container)) {
    if (Metric(R))
      return U(R);
  }
  return Or;
}

template <class T, class U = T>
static auto bestMatchOr(const std::vector<T> &Container,
                        llvm::unique_function<uint64_t(T)> Metric, U Or = U{},
                        bool UpdateEqual = true) -> U {
  auto BestIt = Container.end();
  auto It = Container.begin();
  decltype(Metric(T{})) BestValue = 0;
  for (const auto &R : Container) {
    auto M = Metric(R);
    if (M > BestValue || (M == BestValue && UpdateEqual)) {
      BestIt = It;
      BestValue = M;
    }
    It++;
  }
  return BestIt == Container.end() ? Or : U(*BestIt);
}
