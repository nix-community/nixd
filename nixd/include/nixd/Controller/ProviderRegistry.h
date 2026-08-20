#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/strand.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nixd {

class AttrSetClient;
struct ProviderRecord;
struct ProviderRegistryState;

enum class ProviderKind { Nixpkgs, Option };

struct ProviderKey {
  ProviderKind Kind = ProviderKind::Nixpkgs;
  std::string Name;

  static ProviderKey nixpkgs() { return {}; }
  static ProviderKey option(std::string Name) {
    return {.Kind = ProviderKind::Option, .Name = std::move(Name)};
  }

  auto operator<=>(const ProviderKey &) const = default;
};

enum class ProviderState { Pending, Active, Failed, Retired };

enum class ProviderApplyResult { Ready, Empty, Failed, Superseded, Stopped };

struct ProviderSpec {
  std::optional<std::string> Nixpkgs;
  std::map<std::string, std::string> Options;
};

class ProviderWorker {
public:
  using EvaluationCallback = std::function<void(bool)>;
  using DeathCallback = std::function<void()>;

  virtual ~ProviderWorker() = default;
  virtual void evaluate(std::string Expression, EvaluationCallback Reply) = 0;
  virtual void cancel() = 0;
  [[nodiscard]] virtual bool alive() const = 0;
  [[nodiscard]] virtual AttrSetClient *attrSetClient() { return nullptr; }
};

class ProviderRegistry {
public:
  /// A type-erased strand. ProviderRegistry always dispatches with
  /// boost::asio::post, so worker callbacks cannot run registry work inline.
  using Executor = boost::asio::strand<boost::asio::any_io_executor>;
  using WorkerFactory = std::function<std::shared_ptr<ProviderWorker>(
      const ProviderKey &, const std::filesystem::path &,
      ProviderWorker::DeathCallback)>;
  using ApplyCallback = std::function<void(ProviderApplyResult)>;

  struct Epochs {
    uint64_t Nixpkgs = 0;
    uint64_t Options = 0;

    auto operator<=>(const Epochs &) const = default;
  };

  class QueryToken {
    std::shared_ptr<ProviderRecord> Record;
    std::shared_ptr<ProviderWorker> Worker;
    uint64_t Revision = 0;
    uint64_t ProcessSerial = 0;
    uint64_t Epoch = 0;

    QueryToken(std::shared_ptr<ProviderRecord> Record,
               std::shared_ptr<ProviderWorker> Worker, uint64_t Revision,
               uint64_t ProcessSerial, uint64_t Epoch)
        : Record(std::move(Record)), Worker(std::move(Worker)),
          Revision(Revision), ProcessSerial(ProcessSerial), Epoch(Epoch) {}

    friend class ProviderRegistry;

  public:
    [[nodiscard]] ProviderState observedState() const;
    [[nodiscard]] ProviderKey key() const;
    [[nodiscard]] AttrSetClient *client() const;
    [[nodiscard]] const std::shared_ptr<ProviderWorker> &worker() const {
      return Worker;
    }
  };

  class OptionsSnapshot {
    std::vector<QueryToken> Tokens;
    uint64_t Epoch = 0;

    friend class ProviderRegistry;

  public:
    using const_iterator = std::vector<QueryToken>::const_iterator;

    [[nodiscard]] const_iterator begin() const { return Tokens.begin(); }
    [[nodiscard]] const_iterator end() const { return Tokens.end(); }
    [[nodiscard]] size_t size() const { return Tokens.size(); }
    [[nodiscard]] const QueryToken &operator[](size_t Index) const {
      return Tokens[Index];
    }
  };

private:
  std::shared_ptr<ProviderRegistryState> Shared;

public:
  ProviderRegistry(Executor Post, WorkerFactory Factory,
                   std::filesystem::path StartupCWD);

  /// Publish Spec synchronously so existing query tokens are invalidated before
  /// this call returns. OnApplied is always invoked later on the registry
  /// strand and describes only the exact revisions published by this call.
  void apply(ProviderSpec Spec, ApplyCallback OnApplied = {});
  [[nodiscard]] std::optional<QueryToken> acquire(const ProviderKey &Key) const;
  [[nodiscard]] OptionsSnapshot acquireOptions() const;
  [[nodiscard]] bool validate(const QueryToken &Token) const;
  [[nodiscard]] bool validate(const OptionsSnapshot &Snapshot) const;
  void queryFailed(const QueryToken &Token) const;
  [[nodiscard]] Epochs epochs() const;
  [[nodiscard]] std::optional<ProviderState>
  state(const ProviderKey &Key) const;
  [[nodiscard]] size_t size() const;
  [[nodiscard]] bool accepting() const;
  void shutdown(std::function<void()> OnRetired = {});
};

} // namespace nixd
