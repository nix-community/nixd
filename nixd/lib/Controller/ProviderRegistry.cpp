#include "nixd/Controller/ProviderRegistry.h"

#include <boost/asio/post.hpp>

#include <algorithm>
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
  bool AutomaticRecoveryAvailable = true;
  std::atomic<ProviderState> State{ProviderState::Pending};
};

struct ProviderApplyTicket {
  struct ExactRevision {
    std::shared_ptr<ProviderRecord> Record;
    uint64_t Revision = 0;
  };

  std::vector<ExactRevision> Revisions;
  std::optional<ProviderApplyResult> ForcedResult;
  ProviderRegistry::ApplyCallback Reply;
};

struct ProviderRegistryState {
  enum class ShutdownPhase { Accepting, Stopping, Retired };

  mutable std::mutex Mutex;
  std::map<ProviderKey, std::shared_ptr<ProviderRecord>> Records;
  ProviderRegistry::Epochs EpochValues;
  uint64_t NextProcessSerial = 1;
  std::atomic<ShutdownPhase> Phase{ShutdownPhase::Accepting};
  std::vector<std::shared_ptr<ProviderApplyTicket>> ApplyTickets;
  std::vector<std::function<void()>> ShutdownWaiters;
  ProviderRegistry::Executor Strand;
  ProviderRegistry::WorkerFactory Factory;
  std::filesystem::path StartupCWD;
  ProcessTreeBackend CancellationBackend;

  ProviderRegistryState(ProviderRegistry::Executor Strand,
                        ProviderRegistry::WorkerFactory Factory,
                        std::filesystem::path StartupCWD,
                        ProcessTreeBackend CancellationBackend)
      : Strand(std::move(Strand)), Factory(std::move(Factory)),
        StartupCWD(std::move(StartupCWD)),
        CancellationBackend(std::move(CancellationBackend)) {}
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

void runApplyTickets(const std::shared_ptr<ProviderRegistryState> &Shared) {
  std::vector<std::pair<ProviderRegistry::ApplyCallback, ProviderApplyResult>>
      Replies;
  {
    std::lock_guard Guard(Shared->Mutex);
    // Stopping tickets retire with the records in the single shutdown strand
    // closure. This keeps their callbacks ordered before shutdown waiters, but
    // never before retirement has actually happened.
    if (Shared->Phase == ProviderRegistryState::ShutdownPhase::Stopping)
      return;

    for (auto It = Shared->ApplyTickets.begin();
         It != Shared->ApplyTickets.end();) {
      const auto &Ticket = *It;
      std::optional<ProviderApplyResult> Result = Ticket->ForcedResult;
      if (!Result &&
          Shared->Phase == ProviderRegistryState::ShutdownPhase::Retired) {
        Result = ProviderApplyResult::Stopped;
      }
      if (!Result && Ticket->Revisions.empty())
        Result = ProviderApplyResult::Empty;

      bool AllActive = true;
      bool AnyFailed = false;
      for (const auto &Exact : Ticket->Revisions) {
        if (Result)
          break;
        if (!Exact.Record || !isCurrent(*Shared, Exact.Record) ||
            Exact.Record->Revision != Exact.Revision) {
          Result = ProviderApplyResult::Superseded;
          break;
        }
        if (Exact.Record->State == ProviderState::Failed)
          AnyFailed = true;
        if (Exact.Record->State != ProviderState::Active)
          AllActive = false;
      }
      if (!Result)
        Result = AnyFailed
                     ? std::optional(ProviderApplyResult::Failed)
                     : (AllActive ? std::optional(ProviderApplyResult::Ready)
                                  : std::nullopt);

      if (!Result) {
        ++It;
        continue;
      }
      Replies.emplace_back(std::move(Ticket->Reply), *Result);
      It = Shared->ApplyTickets.erase(It);
    }
  }
  for (auto &[Reply, Result] : Replies)
    Reply(Result);
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
  bool Failed = false;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (isAccepting(*Shared) && isCurrent(*Shared, Record) &&
        Record->State == ProviderState::Pending &&
        Record->Revision == Revision &&
        Record->ProcessSerial == ProcessSerial) {
      Record->State = ProviderState::Failed;
      Failed = true;
    }
  }
  if (Failed)
    runApplyTickets(Shared);
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

            {
              std::lock_guard Guard(Shared->Mutex);
              if (!isAccepting(*Shared) || !isCurrent(*Shared, Record) ||
                  Record->State != ProviderState::Pending ||
                  Record->Revision != Revision ||
                  Record->ProcessSerial != ProcessSerial)
                return;

              if (!Success) {
                Record->State = ProviderState::Failed;
              } else {
                Record->State = ProviderState::Active;
                ++epochFor(*Shared, Record->Key.Kind);
              }
            }
            runApplyTickets(Shared);
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
  bool Failed = false;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (!isAccepting(*Shared) || !isCurrent(*Shared, Record) ||
        Record->State != ProviderState::Pending ||
        Record->Revision != Revision ||
        Record->ProcessSerial != ProcessSerial) {
      StaleWorker = std::move(Worker);
    } else if (!Worker) {
      Record->State = ProviderState::Failed;
      Failed = true;
    } else {
      Record->Worker = Worker;
    }
  }
  if (StaleWorker) {
    cancelNoThrow(StaleWorker);
    return;
  }
  if (Failed)
    runApplyTickets(Shared);
  if (Worker)
    dispatchEvaluation(Shared, Record);
}

void evaluatePending(const std::shared_ptr<ProviderRegistryState> &Shared,
                     const std::shared_ptr<ProviderRecord> &Record,
                     uint64_t Revision) {
  std::shared_ptr<ProviderWorker> Existing;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (!isAccepting(*Shared) || !isCurrent(*Shared, Record) ||
        Record->State != ProviderState::Pending || Record->Revision != Revision)
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

struct ApplyWork {
  std::vector<std::pair<std::shared_ptr<ProviderRecord>, uint64_t>> Evaluate;
  std::vector<std::shared_ptr<ProviderWorker>> Cancel;
};

ApplyWork publishSpec(const std::shared_ptr<ProviderRegistryState> &Shared,
                      ProviderSpec Spec,
                      ProviderRegistry::ApplyCallback OnApplied) {
  std::map<ProviderKey, std::string> Desired;
  if (Spec.Nixpkgs && !Spec.Nixpkgs->empty())
    Desired.emplace(ProviderKey::nixpkgs(), std::move(*Spec.Nixpkgs));
  for (auto &[Name, Expression] : Spec.Options) {
    if (!Expression.empty())
      Desired.emplace(ProviderKey::option(std::move(Name)),
                      std::move(Expression));
  }

  ApplyWork Work;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (!isAccepting(*Shared)) {
      if (OnApplied) {
        auto Ticket = std::make_shared<ProviderApplyTicket>();
        Ticket->Reply = std::move(OnApplied);
        Ticket->ForcedResult = ProviderApplyResult::Stopped;
        Shared->ApplyTickets.push_back(std::move(Ticket));
      }
      return Work;
    }

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
        Work.Cancel.push_back(std::move(Record->Worker));
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
        Work.Evaluate.emplace_back(Record, Record->Revision);
        continue;
      }

      auto &Record = It->second;
      const bool Changed = Record->DesiredExpression != Expression;
      const bool Retry = Record->State == ProviderState::Failed;
      if (!Changed && !Retry)
        continue;
      Record->DesiredExpression = std::move(Expression);
      Record->State = ProviderState::Pending;
      Record->AutomaticRecoveryAvailable = true;
      ++Record->Revision;
      if (Changed)
        ++epochFor(*Shared, Key.Kind);
      Work.Evaluate.emplace_back(Record, Record->Revision);
    }

    if (OnApplied) {
      auto Ticket = std::make_shared<ProviderApplyTicket>();
      Ticket->Reply = std::move(OnApplied);
      Ticket->Revisions.reserve(Desired.size());
      for (const auto &[Key, _] : Desired) {
        const auto It = Shared->Records.find(Key);
        assert(It != Shared->Records.end());
        Ticket->Revisions.push_back({It->second, It->second->Revision});
      }
      Shared->ApplyTickets.push_back(std::move(Ticket));
    }
  }

  return Work;
}

void runApplyWork(const std::shared_ptr<ProviderRegistryState> &Shared,
                  ApplyWork Work) {
  for (auto &Worker : Work.Cancel)
    cancelNoThrow(Worker);
  for (auto &[Record, Revision] : Work.Evaluate)
    evaluatePending(Shared, Record, Revision);
  runApplyTickets(Shared);
}

void recoverDead(const std::shared_ptr<ProviderRegistryState> &Shared,
                 const std::shared_ptr<ProviderRecord> &Record,
                 uint64_t ProcessSerial) {
  std::shared_ptr<ProviderWorker> DeadWorker;
  bool Recover = false;
  {
    std::lock_guard Guard(Shared->Mutex);
    if (!isAccepting(*Shared) || !isCurrent(*Shared, Record) ||
        Record->ProcessSerial != ProcessSerial)
      return;
    if (Record->State == ProviderState::Pending) {
      for (const auto &Ticket : Shared->ApplyTickets) {
        for (const auto &Exact : Ticket->Revisions) {
          if (Exact.Record == Record && Exact.Revision == Record->Revision) {
            Ticket->ForcedResult = ProviderApplyResult::Failed;
            break;
          }
        }
      }
    }
    Recover = Record->AutomaticRecoveryAvailable;
    Record->AutomaticRecoveryAvailable = false;
    Record->State = Recover ? ProviderState::Pending : ProviderState::Failed;
    ++Record->Revision;
    ++epochFor(*Shared, Record->Key.Kind);
    DeadWorker = std::move(Record->Worker);
  }
  if (DeadWorker)
    cancelNoThrow(DeadWorker);
  runApplyTickets(Shared);
  if (Recover)
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
                                   std::filesystem::path StartupCWD,
                                   ProcessTreeBackend CancellationBackend)
    : Shared(std::make_shared<ProviderRegistryState>(
          std::move(Post), Factory, std::move(StartupCWD),
          std::move(CancellationBackend))) {
  assert(Factory);
}

void ProviderRegistry::apply(ProviderSpec Spec, ApplyCallback OnApplied) {
  const bool HasCallback = static_cast<bool>(OnApplied);
  auto Work = publishSpec(Shared, std::move(Spec), std::move(OnApplied));
  if (Work.Cancel.empty() && Work.Evaluate.empty() && !HasCallback)
    return;
  auto State = Shared;
  postDeferred(Shared,
               [State = std::move(State), Work = std::move(Work)]() mutable {
                 runApplyWork(State, std::move(Work));
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

ProviderRegistry::OptionsSnapshot ProviderRegistry::acquireOptions() const {
  OptionsSnapshot Snapshot;
  {
    std::lock_guard Guard(Shared->Mutex);
    Snapshot.Epoch = Shared->EpochValues.Options;
    if (!isAccepting(*Shared))
      return Snapshot;
    for (const auto &[Key, Record] : Shared->Records) {
      if (Key.Kind != ProviderKind::Option ||
          Record->State != ProviderState::Active || !Record->Worker)
        continue;
      Snapshot.Tokens.push_back(
          QueryToken(Record, Record->Worker, Record->Revision,
                     Record->ProcessSerial, Snapshot.Epoch));
    }
  }

  for (auto It = Snapshot.Tokens.begin(); It != Snapshot.Tokens.end();) {
    if (It->Worker->alive()) {
      ++It;
      continue;
    }
    queryFailed(*It);
    It = Snapshot.Tokens.erase(It);
  }
  return Snapshot;
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

bool ProviderRegistry::validate(const OptionsSnapshot &Snapshot) const {
  {
    std::lock_guard Guard(Shared->Mutex);
    if (!isAccepting(*Shared) || Shared->EpochValues.Options != Snapshot.Epoch)
      return false;

    size_t ActiveOptions = 0;
    for (const auto &[Key, Record] : Shared->Records) {
      if (Key.Kind != ProviderKind::Option ||
          Record->State != ProviderState::Active || !Record->Worker)
        continue;
      ++ActiveOptions;
      const auto It =
          std::find_if(Snapshot.Tokens.begin(), Snapshot.Tokens.end(),
                       [&](const QueryToken &Token) {
                         return Token.Record == Record &&
                                Token.Worker == Record->Worker &&
                                Token.Revision == Record->Revision &&
                                Token.ProcessSerial == Record->ProcessSerial &&
                                Token.Epoch == Snapshot.Epoch;
                       });
      if (It == Snapshot.Tokens.end())
        return false;
    }
    if (ActiveOptions != Snapshot.Tokens.size())
      return false;
  }

  for (const auto &Token : Snapshot.Tokens) {
    if (Token.Worker->alive())
      continue;
    queryFailed(Token);
    return false;
  }
  return true;
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
  // are released and cannot keep pool work blocked. Real evaluator workers
  // split stop into prepare/finish so every process tree shares this one grace
  // deadline; the default hook still calls fake workers synchronously once.
  std::vector<std::shared_ptr<ProviderWorker>> CoordinatedWorkers;
  std::vector<std::shared_ptr<ProcessTreeIdentity>> ProcessTrees;
  CoordinatedWorkers.reserve(Workers.size());
  ProcessTrees.reserve(Workers.size());
  for (auto &Worker : Workers) {
    if (!Worker)
      continue;
    try {
      if (auto Identity = Worker->prepareCancellation()) {
        CoordinatedWorkers.push_back(Worker);
        ProcessTrees.push_back(std::move(Identity));
      }
    } catch (...) {
      // Shutdown is no-throw and continues retiring independent providers.
    }
  }
  cancelProcessTrees(ProcessTrees, Shared->CancellationBackend);
  for (auto &Worker : CoordinatedWorkers)
    Worker->finishCancellation();

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
    runApplyTickets(State);
    for (auto &Waiter : Waiters)
      Waiter();
  });
}

ProviderState ProviderRegistry::QueryToken::observedState() const {
  assert(Record);
  return Record->State;
}

ProviderKey ProviderRegistry::QueryToken::key() const {
  assert(Record);
  return Record->Key;
}

AttrSetClient *ProviderRegistry::QueryToken::client() const {
  return Worker ? Worker->attrSetClient() : nullptr;
}

} // namespace nixd
