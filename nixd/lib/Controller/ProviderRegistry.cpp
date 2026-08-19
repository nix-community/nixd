#include "nixd/Controller/ProviderRegistry.h"

#include <boost/asio/post.hpp>

#include <cassert>
#include <mutex>
#include <utility>
#include <vector>

namespace nixd {

struct ProviderRecord {
  ProviderKey Key;
  std::string DesiredExpression;
  std::shared_ptr<ProviderWorker> Worker;
  uint64_t Revision = 0;
  uint64_t ProcessSerial = 0;
  std::atomic<ProviderState> State{ProviderState::Pending};
};

struct ProviderRegistryState {
  enum class ShutdownPhase { Accepting, Stopping, Retired };

  mutable std::mutex Mutex;
  std::map<ProviderKey, std::shared_ptr<ProviderRecord>> Records;
  ProviderRegistry::Epochs EpochValues;
  uint64_t NextProcessSerial = 1;
  std::atomic<ShutdownPhase> Phase{ShutdownPhase::Accepting};
  std::vector<std::function<void()>> ShutdownWaiters;
  ProviderRegistry::Executor Strand;
  ProviderRegistry::WorkerFactory Factory;
  std::filesystem::path StartupCWD;

  ProviderRegistryState(ProviderRegistry::Executor Strand,
                        ProviderRegistry::WorkerFactory Factory,
                        std::filesystem::path StartupCWD)
      : Strand(std::move(Strand)), Factory(std::move(Factory)),
        StartupCWD(std::move(StartupCWD)) {}
};

namespace {

bool isAccepting(const ProviderRegistryState &Shared) {
  return Shared.Phase == ProviderRegistryState::ShutdownPhase::Accepting;
}

template <typename Action>
void postDeferred(const std::shared_ptr<ProviderRegistryState> &Shared,
                  Action &&Fn) {
  boost::asio::post(Shared->Strand, std::forward<Action>(Fn));
}

uint64_t &epochFor(ProviderRegistryState &Shared, ProviderKind Kind) {
  return Kind == ProviderKind::Nixpkgs ? Shared.EpochValues.Nixpkgs
                                       : Shared.EpochValues.Options;
}

bool isCurrent(const ProviderRegistryState &Shared,
               const std::shared_ptr<ProviderRecord> &Record) {
  auto It = Shared.Records.find(Record->Key);
  return It != Shared.Records.end() && It->second == Record;
}

void cancelNoThrow(const std::shared_ptr<ProviderWorker> &Worker) noexcept {
  if (!Worker)
    return;
  try {
    Worker->cancel();
  } catch (...) {
    // Retirement and recovery are cancellation boundaries. A worker failure
    // must not prevent other records or shutdown waiters from retiring.
  }
}

void failMatchingPending(const std::shared_ptr<ProviderRegistryState> &Shared,
                         const std::shared_ptr<ProviderRecord> &Record,
                         uint64_t Revision, uint64_t ProcessSerial) {
  std::lock_guard Guard(Shared->Mutex);
  if (isAccepting(*Shared) && isCurrent(*Shared, Record) &&
      Record->State == ProviderState::Pending && Record->Revision == Revision &&
      Record->ProcessSerial == ProcessSerial)
    Record->State = ProviderState::Failed;
}

void postDeath(const std::shared_ptr<ProviderRegistryState> &Shared,
               const std::shared_ptr<ProviderRecord> &Record,
               uint64_t ProcessSerial);

void dispatchEvaluation(const std::shared_ptr<ProviderRegistryState> &Shared,
                        const std::shared_ptr<ProviderRecord> &Record) {
  std::shared_ptr<ProviderWorker> Worker;
  std::string Expression;
  uint64_t Revision = 0;
  uint64_t ProcessSerial = 0;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (!isAccepting(*Shared) || !isCurrent(*Shared, Record) ||
        Record->State != ProviderState::Pending || !Record->Worker)
      return;
    Worker = Record->Worker;
    Expression = Record->DesiredExpression;
    Revision = Record->Revision;
    ProcessSerial = Record->ProcessSerial;
  }

  std::weak_ptr<ProviderRegistryState> WeakShared = Shared;
  std::weak_ptr<ProviderRecord> WeakRecord = Record;
  auto Strand = Shared->Strand;
  try {
    Worker->evaluate(
        std::move(Expression),
        [Strand = std::move(Strand), WeakShared = std::move(WeakShared),
         WeakRecord = std::move(WeakRecord), Revision,
         ProcessSerial](bool Success) mutable {
          // This callback can run on the worker input thread. Do not
          // lock either weak owner until the injected safe executor
          // runs the completion.
          boost::asio::post(Strand, [WeakShared = std::move(WeakShared),
                                     WeakRecord = std::move(WeakRecord),
                                     Revision, ProcessSerial, Success] {
            auto Shared = WeakShared.lock();
            auto Record = WeakRecord.lock();
            if (!Shared || !Record)
              return;

            std::lock_guard Guard(Shared->Mutex);
            if (!isAccepting(*Shared) || !isCurrent(*Shared, Record) ||
                Record->State != ProviderState::Pending ||
                Record->Revision != Revision ||
                Record->ProcessSerial != ProcessSerial)
              return;

            if (!Success) {
              Record->State = ProviderState::Failed;
              return;
            }
            Record->State = ProviderState::Active;
            ++epochFor(*Shared, Record->Key.Kind);
          });
        });
  } catch (...) {
    failMatchingPending(Shared, Record, Revision, ProcessSerial);
  }
}

void createWorkerAndEvaluate(
    const std::shared_ptr<ProviderRegistryState> &Shared,
    const std::shared_ptr<ProviderRecord> &Record) {
  uint64_t Revision = 0;
  uint64_t ProcessSerial = 0;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (!isAccepting(*Shared) || !isCurrent(*Shared, Record) ||
        Record->State != ProviderState::Pending)
      return;
    Revision = Record->Revision;
    ProcessSerial = Shared->NextProcessSerial++;
    Record->ProcessSerial = ProcessSerial;
  }

  std::weak_ptr<ProviderRegistryState> WeakShared = Shared;
  std::weak_ptr<ProviderRecord> WeakRecord = Record;
  auto Strand = Shared->Strand;
  auto OnDeath = [Strand = std::move(Strand), WeakShared, WeakRecord,
                  ProcessSerial]() mutable {
    // Like evaluation replies, transport death is reported on the input
    // thread and merely posts weak identity data to the safe executor.
    boost::asio::post(Strand, [WeakShared, WeakRecord, ProcessSerial] {
      auto Shared = WeakShared.lock();
      auto Record = WeakRecord.lock();
      if (Shared && Record)
        postDeath(Shared, Record, ProcessSerial);
    });
  };

  std::shared_ptr<ProviderWorker> Worker;
  try {
    Worker =
        Shared->Factory(Record->Key, Shared->StartupCWD, std::move(OnDeath));
  } catch (...) {
    failMatchingPending(Shared, Record, Revision, ProcessSerial);
    return;
  }
  std::shared_ptr<ProviderWorker> StaleWorker;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (!isAccepting(*Shared) || !isCurrent(*Shared, Record) ||
        Record->State != ProviderState::Pending ||
        Record->Revision != Revision ||
        Record->ProcessSerial != ProcessSerial) {
      StaleWorker = std::move(Worker);
    } else if (!Worker) {
      Record->State = ProviderState::Failed;
    } else {
      Record->Worker = Worker;
    }
  }
  if (StaleWorker) {
    cancelNoThrow(StaleWorker);
    return;
  }
  if (Worker)
    dispatchEvaluation(Shared, Record);
}

void evaluatePending(const std::shared_ptr<ProviderRegistryState> &Shared,
                     const std::shared_ptr<ProviderRecord> &Record) {
  std::shared_ptr<ProviderWorker> Existing;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (!isAccepting(*Shared) || !isCurrent(*Shared, Record) ||
        Record->State != ProviderState::Pending)
      return;
    Existing = Record->Worker;
  }

  if (Existing && Existing->alive()) {
    dispatchEvaluation(Shared, Record);
    return;
  }

  if (Existing) {
    {
      std::lock_guard Guard(Shared->Mutex);
      if (isCurrent(*Shared, Record) && Record->Worker == Existing)
        Record->Worker.reset();
    }
    cancelNoThrow(Existing);
  }
  createWorkerAndEvaluate(Shared, Record);
}

void applySpec(const std::shared_ptr<ProviderRegistryState> &Shared,
               ProviderSpec Spec) {
  std::map<ProviderKey, std::string> Desired;
  if (Spec.Nixpkgs && !Spec.Nixpkgs->empty())
    Desired.emplace(ProviderKey::nixpkgs(), std::move(*Spec.Nixpkgs));
  for (auto &[Name, Expression] : Spec.Options) {
    if (!Expression.empty())
      Desired.emplace(ProviderKey::option(std::move(Name)),
                      std::move(Expression));
  }

  std::vector<std::shared_ptr<ProviderRecord>> Evaluate;
  std::vector<std::shared_ptr<ProviderWorker>> Cancel;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (!isAccepting(*Shared))
      return;

    for (auto It = Shared->Records.begin(); It != Shared->Records.end();) {
      if (Desired.contains(It->first)) {
        ++It;
        continue;
      }
      auto Record = It->second;
      Record->State = ProviderState::Retired;
      ++Record->Revision;
      ++epochFor(*Shared, Record->Key.Kind);
      if (Record->Worker)
        Cancel.push_back(std::move(Record->Worker));
      It = Shared->Records.erase(It);
    }

    for (auto &[Key, Expression] : Desired) {
      auto It = Shared->Records.find(Key);
      if (It == Shared->Records.end()) {
        auto Record = std::make_shared<ProviderRecord>();
        Record->Key = Key;
        Record->DesiredExpression = std::move(Expression);
        Record->Revision = 1;
        Record->State = ProviderState::Pending;
        Shared->Records.emplace(Key, Record);
        ++epochFor(*Shared, Key.Kind);
        Evaluate.push_back(std::move(Record));
        continue;
      }

      auto &Record = It->second;
      const bool Changed = Record->DesiredExpression != Expression;
      const bool Retry = Record->State == ProviderState::Failed;
      if (!Changed && !Retry)
        continue;
      Record->DesiredExpression = std::move(Expression);
      Record->State = ProviderState::Pending;
      ++Record->Revision;
      if (Changed)
        ++epochFor(*Shared, Key.Kind);
      Evaluate.push_back(Record);
    }
  }

  for (auto &Worker : Cancel)
    cancelNoThrow(Worker);
  for (auto &Record : Evaluate)
    evaluatePending(Shared, Record);
}

void recoverDead(const std::shared_ptr<ProviderRegistryState> &Shared,
                 const std::shared_ptr<ProviderRecord> &Record,
                 uint64_t ProcessSerial) {
  std::shared_ptr<ProviderWorker> DeadWorker;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (!isAccepting(*Shared) || !isCurrent(*Shared, Record) ||
        Record->ProcessSerial != ProcessSerial)
      return;
    Record->State = ProviderState::Pending;
    ++Record->Revision;
    ++epochFor(*Shared, Record->Key.Kind);
    DeadWorker = std::move(Record->Worker);
  }
  if (DeadWorker)
    cancelNoThrow(DeadWorker);
  createWorkerAndEvaluate(Shared, Record);
}

void postDeath(const std::shared_ptr<ProviderRegistryState> &Shared,
               const std::shared_ptr<ProviderRecord> &Record,
               uint64_t ProcessSerial) {
  // postDeath is entered only by a closure already running on the executor.
  recoverDead(Shared, Record, ProcessSerial);
}

} // namespace

ProviderRegistry::ProviderRegistry(Executor Post, WorkerFactory Factory,
                                   std::filesystem::path StartupCWD)
    : Shared(std::make_shared<ProviderRegistryState>(std::move(Post), Factory,
                                                     std::move(StartupCWD))) {
  assert(Factory);
}

void ProviderRegistry::apply(ProviderSpec Spec) {
  if (!isAccepting(*Shared))
    return;
  auto State = Shared;
  postDeferred(Shared,
               [State = std::move(State), Spec = std::move(Spec)]() mutable {
                 applySpec(State, std::move(Spec));
               });
}

std::optional<ProviderRegistry::QueryToken>
ProviderRegistry::acquire(const ProviderKey &Key) const {
  std::shared_ptr<ProviderRecord> Record;
  std::shared_ptr<ProviderWorker> Worker;
  uint64_t Revision = 0;
  uint64_t ProcessSerial = 0;
  uint64_t Epoch = 0;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (!isAccepting(*Shared))
      return std::nullopt;
    auto It = Shared->Records.find(Key);
    if (It == Shared->Records.end() ||
        It->second->State != ProviderState::Active || !It->second->Worker)
      return std::nullopt;
    Record = It->second;
    Worker = Record->Worker;
    Revision = Record->Revision;
    ProcessSerial = Record->ProcessSerial;
    Epoch = epochFor(*Shared, Key.Kind);
  }
  if (!Worker->alive()) {
    auto State = Shared;
    postDeferred(State, [State, Record, ProcessSerial] {
      recoverDead(State, Record, ProcessSerial);
    });
    return std::nullopt;
  }
  return QueryToken(std::move(Record), std::move(Worker), Revision,
                    ProcessSerial, Epoch);
}

bool ProviderRegistry::validate(const QueryToken &Token) const {
  bool Valid = false;
  {
    std::lock_guard Guard(Shared->Mutex);
    Valid = isAccepting(*Shared) && Token.Record && Token.Worker &&
            isCurrent(*Shared, Token.Record) &&
            Token.Record->State == ProviderState::Active &&
            Token.Record->Revision == Token.Revision &&
            Token.Record->ProcessSerial == Token.ProcessSerial &&
            Token.Record->Worker == Token.Worker &&
            epochFor(*Shared, Token.Record->Key.Kind) == Token.Epoch;
  }
  if (!Valid)
    return false;
  if (Token.Worker->alive())
    return true;
  queryFailed(Token);
  return false;
}

void ProviderRegistry::queryFailed(const QueryToken &Token) const {
  if (!Token.Record || !Token.Worker || Token.Worker->alive())
    return;
  auto State = Shared;
  auto Record = Token.Record;
  const auto ProcessSerial = Token.ProcessSerial;
  postDeferred(State, [State, Record = std::move(Record), ProcessSerial] {
    recoverDead(State, Record, ProcessSerial);
  });
}

ProviderRegistry::Epochs ProviderRegistry::epochs() const {
  std::lock_guard Guard(Shared->Mutex);
  return Shared->EpochValues;
}

std::optional<ProviderState>
ProviderRegistry::state(const ProviderKey &Key) const {
  std::lock_guard Guard(Shared->Mutex);
  auto It = Shared->Records.find(Key);
  if (It == Shared->Records.end())
    return std::nullopt;
  return It->second->State.load();
}

size_t ProviderRegistry::size() const {
  std::lock_guard Guard(Shared->Mutex);
  return Shared->Records.size();
}

bool ProviderRegistry::accepting() const { return isAccepting(*Shared); }

void ProviderRegistry::shutdown(std::function<void()> OnRetired) {
  std::vector<std::shared_ptr<ProviderWorker>> Workers;
  bool StartRetirement = false;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (Shared->Phase == ProviderRegistryState::ShutdownPhase::Retired) {
      if (OnRetired)
        postDeferred(Shared, std::move(OnRetired));
      return;
    }
    if (OnRetired)
      Shared->ShutdownWaiters.push_back(std::move(OnRetired));
    if (Shared->Phase == ProviderRegistryState::ShutdownPhase::Accepting) {
      Shared->Phase = ProviderRegistryState::ShutdownPhase::Stopping;
      StartRetirement = true;
      for (const auto &[_, Record] : Shared->Records) {
        if (Record->Worker)
          Workers.push_back(Record->Worker);
      }
    }
  }
  if (!StartRetirement)
    return;

  // Cancel before waiting for the executor so pending worker RPC callbacks
  // are released and cannot keep pool work blocked.
  for (auto &Worker : Workers)
    cancelNoThrow(Worker);

  auto State = Shared;
  postDeferred(Shared, [State = std::move(State)]() mutable {
    std::vector<std::shared_ptr<ProviderWorker>> Release;
    std::vector<std::function<void()>> Waiters;
    {
      std::lock_guard Guard(State->Mutex);
      for (auto &[_, Record] : State->Records) {
        Record->State = ProviderState::Retired;
        ++Record->Revision;
        if (Record->Worker)
          Release.push_back(std::move(Record->Worker));
      }
      State->Records.clear();
      State->Phase = ProviderRegistryState::ShutdownPhase::Retired;
      Waiters.swap(State->ShutdownWaiters);
    }
    Release.clear();
    for (auto &Waiter : Waiters)
      Waiter();
  });
}

ProviderState ProviderRegistry::QueryToken::observedState() const {
  assert(Record);
  return Record->State;
}

} // namespace nixd
