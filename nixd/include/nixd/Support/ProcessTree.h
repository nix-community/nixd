#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <sys/types.h>

namespace nixd {

struct ProcessTreeBackend {
  std::function<int(pid_t, int)> Kill;
  std::function<void(std::chrono::milliseconds)> WaitForGrace;
  /// Returns 1 for an exited-but-unreaped leader, 0 while running, and -1
  /// after an external reap. Tests may omit this system-only observation.
  std::function<int(pid_t)> ObserveLeader;
  std::function<std::chrono::steady_clock::time_point()> Now;
  std::function<pid_t(pid_t, int *, int)> WaitPID;

  static ProcessTreeBackend system();
};

/// A direct child remains the identity barrier for its dedicated process group
/// until the sole waitpid owner reaps it through this object.
class ProcessTreeIdentity {
  mutable std::mutex Mutex;
  std::condition_variable CancellationChanged;
  pid_t PID = -1;
  pid_t ProcessGroup = -1;
  bool LeaderExited = false;
  bool Reaped = false;
  bool CancellationStarted = false;
  bool CancellationFinished = false;

  [[nodiscard]] pid_t signalTargetLocked() const;
  [[nodiscard]] bool ownsDedicatedGroupLocked() const;
  [[nodiscard]] bool
  isAliveLocked(const ProcessTreeBackend &Backend) const noexcept;

public:
  ProcessTreeIdentity() = default;
  ProcessTreeIdentity(pid_t PID, pid_t ProcessGroup);

  void setProcess(pid_t PID, pid_t ProcessGroup);
  [[nodiscard]] pid_t pid() const;
  [[nodiscard]] bool cancellationRequested() const;
  [[nodiscard]] bool ownsIdentity() const;
  [[nodiscard]] bool ownsDedicatedGroup() const;

  /// Returns true only to the caller responsible for TERM/grace/KILL.
  bool beginCancellation();
  void finishCancellation();
  void waitForCancellation();

  /// Signal only while the direct leader is still an unreaped identity
  /// barrier. A validated dedicated group is preferred over the direct PID.
  bool signalIfAlive(int Signal, const ProcessTreeBackend &Backend) noexcept;
  [[nodiscard]] bool isAlive(const ProcessTreeBackend &Backend) const noexcept;

  /// Non-reaping observation used by evaluator liveness checks.
  bool observeLeaderExit() noexcept;
  bool observeLeaderExit(const ProcessTreeBackend &Backend) noexcept;

  /// A successfully completed direct child no longer needs graceful shutdown.
  /// While its unreaped PID still pins a validated dedicated PGID, send TERM
  /// and immediately KILL any background members. Direct-PID fallback owns no
  /// descendant identity and is left for the sole reap owner.
  void terminateCompletedOwnedGroup(const ProcessTreeBackend &Backend =
                                        ProcessTreeBackend::system()) noexcept;

  /// The sole owner calls this instead of waitpid so group signaling and reap
  /// cannot cross. After KILL, only an uninterruptible kernel D-state can keep
  /// a blocking owner reap from completing. Returns waitpid's result.
  pid_t reapChild(int &Status, int Options,
                  const ProcessTreeBackend &Backend =
                      ProcessTreeBackend::system()) noexcept;

  /// Deterministic test seam for fake process backends.
  bool markReaped();
};

/// Cancel a batch with one shared TERM grace interval, then KILL survivors.
void cancelProcessTrees(
    std::span<const std::shared_ptr<ProcessTreeIdentity>> Trees,
    const ProcessTreeBackend &Backend = ProcessTreeBackend::system()) noexcept;

} // namespace nixd
