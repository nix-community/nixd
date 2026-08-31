#include "nixd/Support/ProcessTree.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace nixd;

ProcessTreeBackend ProcessTreeBackend::system() {
  return {
      .Kill = [](pid_t Target, int Signal) { return ::kill(Target, Signal); },
      .WaitForGrace =
          [](std::chrono::milliseconds Grace) {
            std::this_thread::sleep_for(Grace);
          },
      .ObserveLeader =
          [](pid_t PID) {
            siginfo_t Info{};
            int Result;
            do {
              Result = ::waitid(P_PID, PID, &Info, WEXITED | WNOHANG | WNOWAIT);
            } while (Result < 0 && errno == EINTR);
            if (Result == 0 && Info.si_pid == PID)
              return 1;
            return Result < 0 && errno == ECHILD ? -1 : 0;
          },
      .Now = [] { return std::chrono::steady_clock::now(); },
      .WaitPID = [](pid_t PID, int *Status,
                    int Options) { return ::waitpid(PID, Status, Options); },
  };
}

ProcessTreeIdentity::ProcessTreeIdentity(pid_t PID, pid_t ProcessGroup)
    : PID(PID), ProcessGroup(ProcessGroup) {}

void ProcessTreeIdentity::setProcess(pid_t NewPID, pid_t NewProcessGroup) {
  std::lock_guard Guard(Mutex);
  PID = NewPID;
  ProcessGroup = NewProcessGroup;
}

pid_t ProcessTreeIdentity::pid() const {
  std::lock_guard Guard(Mutex);
  return PID;
}

pid_t ProcessTreeIdentity::signalTargetLocked() const {
  return ownsDedicatedGroupLocked() ? -ProcessGroup : PID;
}

bool ProcessTreeIdentity::ownsDedicatedGroupLocked() const {
  return !Reaped && PID > 0 && ProcessGroup == PID &&
         ProcessGroup != ::getpgrp();
}

bool ProcessTreeIdentity::cancellationRequested() const {
  std::lock_guard Guard(Mutex);
  return CancellationStarted;
}

bool ProcessTreeIdentity::ownsIdentity() const {
  std::lock_guard Guard(Mutex);
  return !Reaped;
}

bool ProcessTreeIdentity::ownsDedicatedGroup() const {
  std::lock_guard Guard(Mutex);
  return ownsDedicatedGroupLocked();
}

bool ProcessTreeIdentity::beginCancellation() {
  std::lock_guard Guard(Mutex);
  if (CancellationStarted)
    return false;
  CancellationStarted = true;
  return true;
}

void ProcessTreeIdentity::finishCancellation() {
  {
    std::lock_guard Guard(Mutex);
    CancellationFinished = true;
  }
  CancellationChanged.notify_all();
}

void ProcessTreeIdentity::waitForCancellation() {
  std::unique_lock Lock(Mutex);
  if (!CancellationStarted)
    return;
  CancellationChanged.wait(Lock, [this] { return CancellationFinished; });
}

bool ProcessTreeIdentity::signalIfAlive(
    int Signal, const ProcessTreeBackend &Backend) noexcept {
  std::lock_guard Guard(Mutex);
  if (!isAliveLocked(Backend))
    return false;
  const pid_t Target = signalTargetLocked();
  try {
    errno = 0;
    return Backend.Kill(Target, Signal) == 0 || errno == EPERM;
  } catch (...) {
    return false;
  }
}

bool ProcessTreeIdentity::isAliveLocked(
    const ProcessTreeBackend &Backend) const noexcept {
  if (Reaped || PID <= 0)
    return false;
  // Once a direct-PID fallback leader exits, it owns no durable descendant
  // identity. A dedicated PGID remains safe to signal because the unreaped
  // group leader pins that identity until the sole owner reaps it.
  if (LeaderExited && !ownsDedicatedGroupLocked())
    return false;
  try {
    errno = 0;
    return Backend.Kill(signalTargetLocked(), 0) == 0 || errno == EPERM;
  } catch (...) {
    return false;
  }
}

bool ProcessTreeIdentity::isAlive(
    const ProcessTreeBackend &Backend) const noexcept {
  std::lock_guard Guard(Mutex);
  return isAliveLocked(Backend);
}

bool ProcessTreeIdentity::observeLeaderExit() noexcept {
  std::lock_guard Guard(Mutex);
  if (LeaderExited || Reaped)
    return true;
  siginfo_t Info{};
  int Result;
  do {
    Result = ::waitid(P_PID, PID, &Info, WEXITED | WNOHANG | WNOWAIT);
  } while (Result < 0 && errno == EINTR);
  if (Result == 0 && Info.si_pid == PID) {
    LeaderExited = true;
    return true;
  }
  if (Result < 0 && errno == ECHILD) {
    LeaderExited = true;
    Reaped = true;
    return true;
  }
  return false;
}

bool ProcessTreeIdentity::observeLeaderExit(
    const ProcessTreeBackend &Backend) noexcept {
  std::lock_guard Guard(Mutex);
  if (LeaderExited || Reaped)
    return true;
  if (!Backend.ObserveLeader)
    return false;
  int Observation;
  try {
    Observation = Backend.ObserveLeader(PID);
  } catch (...) {
    return false;
  }
  if (Observation > 0) {
    LeaderExited = true;
    return true;
  }
  if (Observation < 0) {
    LeaderExited = true;
    Reaped = true;
    return true;
  }
  return false;
}

bool ProcessTreeIdentity::terminateCompletedOwnedGroup(
    const ProcessTreeBackend &Backend) noexcept {
  if (!beginCancellation()) {
    waitForCancellation();
    return false;
  }

  {
    std::lock_guard Guard(Mutex);
    if (ownsDedicatedGroupLocked()) {
      try {
        (void)Backend.Kill(-ProcessGroup, SIGTERM);
        (void)Backend.Kill(-ProcessGroup, SIGKILL);
      } catch (...) {
        // Successful-job cleanup is best effort; sole-owner reap still follows.
      }
    }
  }
  finishCancellation();
  return true;
}

pid_t ProcessTreeIdentity::reapChild(
    int &Status, int Options, const ProcessTreeBackend &Backend) noexcept {
  std::lock_guard Guard(Mutex);
  if (Reaped) {
    errno = ECHILD;
    return -1;
  }
  pid_t Result;
  do {
    try {
      Result = Backend.WaitPID ? Backend.WaitPID(PID, &Status, Options)
                               : ::waitpid(PID, &Status, Options);
    } catch (...) {
      errno = EIO;
      Result = -1;
    }
  } while (Result < 0 && errno == EINTR);
  if (Result == PID || (Result < 0 && errno == ECHILD)) {
    LeaderExited = true;
    Reaped = true;
  }
  return Result;
}

bool ProcessTreeIdentity::markReaped() {
  std::lock_guard Guard(Mutex);
  if (Reaped)
    return false;
  LeaderExited = true;
  Reaped = true;
  return true;
}

void nixd::cancelProcessTrees(
    std::span<const std::shared_ptr<ProcessTreeIdentity>> Trees,
    const ProcessTreeBackend &Backend) noexcept {
  std::vector<std::shared_ptr<ProcessTreeIdentity>> Coordinated;
  Coordinated.reserve(Trees.size());
  for (const auto &Tree : Trees) {
    if (Tree && Tree->beginCancellation())
      Coordinated.push_back(Tree);
  }

  if (!Coordinated.empty()) {
    bool SentTermination = false;
    for (const auto &Tree : Coordinated)
      SentTermination =
          Tree->signalIfAlive(SIGTERM, Backend) || SentTermination;
    if (SentTermination) {
      constexpr std::chrono::milliseconds Grace(500);
      constexpr std::chrono::milliseconds PollInterval(10);
      const bool HasClock = static_cast<bool>(Backend.Now);
      auto Now = [&Backend]() noexcept {
        try {
          if (Backend.Now)
            return Backend.Now();
        } catch (...) {
        }
        return std::chrono::steady_clock::now();
      };
      const auto Deadline = Now() + Grace;
      std::chrono::milliseconds Remaining = Grace;
      while (Remaining > std::chrono::milliseconds::zero()) {
        bool AnyAlive = false;
        for (const auto &Tree : Coordinated) {
          if (Tree->observeLeaderExit(Backend)) {
            // Cancellation gives a validated owned group the full shared TERM
            // grace even after its leader exits. The unreaped leader pins PGID
            // against reuse. Direct-PID fallback cannot own descendants and
            // exits early. Successful formatter completion uses the separate
            // immediate owned-group cleanup policy instead.
            AnyAlive = Tree->ownsDedicatedGroup() || AnyAlive;
            continue;
          }
          AnyAlive = Tree->isAlive(Backend) || AnyAlive;
        }
        if (!AnyAlive)
          break;

        if (HasClock) {
          const auto Current = Now();
          if (Current >= Deadline)
            break;
          Remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
              Deadline - Current);
          if (Remaining <= std::chrono::milliseconds::zero())
            break;
        }
        const std::chrono::milliseconds Delay =
            std::min(PollInterval, Remaining);
        try {
          Backend.WaitForGrace(Delay);
        } catch (...) {
          // Cancellation is a no-throw shutdown path; escalation still runs.
          break;
        }
        if (!HasClock)
          Remaining -= Delay;
      }
      for (const auto &Tree : Coordinated)
        (void)Tree->signalIfAlive(SIGKILL, Backend);
    }
    for (const auto &Tree : Coordinated)
      Tree->finishCancellation();
  }

  for (const auto &Tree : Trees) {
    if (Tree)
      Tree->waitForCancellation();
  }
}
