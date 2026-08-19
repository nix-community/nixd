#include "nixd-config.h"

#include "nixd/Eval/AttrSetClient.h"

#include <cerrno>
#include <chrono>
#include <signal.h> // NOLINT(modernize-deprecated-headers)
#include <sys/wait.h>

using namespace nixd;
using namespace lspserver;

AttrSetClient::AttrSetClient(std::unique_ptr<lspserver::InboundPort> In,
                             std::unique_ptr<lspserver::OutboundPort> Out)
    : LSPServer(std::move(In), std::move(Out)) {
  EvalExpr = mkOutMethod<EvalExprParams, EvalExprResponse>(rpcMethod::EvalExpr);
  AttrPathInfo = mkOutMethod<AttrPathInfoParams, AttrPathInfoResponse>(
      rpcMethod::AttrPathInfo);
  AttrPathComplete =
      mkOutMethod<AttrPathCompleteParams, AttrPathCompleteResponse>(
          rpcMethod::AttrPathComplete);
  OptionInfo = mkOutMethod<AttrPathInfoParams, OptionInfoResponse>(
      rpcMethod::OptionInfo);
  OptionComplete = mkOutMethod<AttrPathCompleteParams, OptionCompleteResponse>(
      rpcMethod::OptionComplete);
  Exit = mkOutNotifiction<std::nullptr_t>(rpcMethod::Exit);
}

const char *AttrSetClient::getExe() {
  if (const char *Env = std::getenv("NIXD_ATTRSET_EVAL"))
    return Env;
  return NIXD_LIBEXEC "/nixd-attrset-eval";
}

AttrSetClientProc::AttrSetClientProc(const std::function<int()> &Action,
                                     std::function<void()> OnDeath)
    : Proc(Action), Client(Proc.mkIn(), Proc.mkOut()),
      OnDeath(std::move(OnDeath)), Input([this]() {
        Client.run();
        TransportAlive = false;
        if (this->OnDeath) {
          try {
            this->OnDeath();
          } catch (...) {
            // A process-input callback must never escape the input thread.
          }
        }
      }) {}

AttrSetClientProc::~AttrSetClientProc() {
  // Releasing the final owner from OnDeath would destroy the callback and its
  // thread while both are still executing. Fail explicitly instead of trying
  // to join self or detaching a thread that still references this object.
  if (Input.get_id() == std::this_thread::get_id())
    std::terminate();
  if (!stop())
    std::terminate();
}

AttrSetClient *AttrSetClientProc::client() {
  return alive() ? &Client : nullptr;
}

bool AttrSetClientProc::reapChild() const {
  std::lock_guard Guard(ReapMutex);
  if (ChildReaped)
    return true;

  int Status = 0;
  pid_t Result;
  do {
    Result = ::waitpid(Proc.proc().PID, &Status, 0);
  } while (Result < 0 && errno == EINTR);

  if (Result == Proc.proc().PID || (Result < 0 && errno == ECHILD)) {
    LeaderExited = true;
    ChildReaped = true;
    TransportAlive = false;
    return true;
  }
  return false;
}

bool AttrSetClientProc::observeChildExit() const {
  std::lock_guard Guard(ReapMutex);
  if (LeaderExited || ChildReaped)
    return true;

  siginfo_t Info{};
  int Result;
  do {
    Result =
        ::waitid(P_PID, Proc.proc().PID, &Info, WEXITED | WNOHANG | WNOWAIT);
  } while (Result < 0 && errno == EINTR);

  if (Result == 0 && Info.si_pid == Proc.proc().PID) {
    LeaderExited = true;
    TransportAlive = false;
    return true;
  }
  if (Result < 0 && errno == ECHILD) {
    LeaderExited = true;
    ChildReaped = true;
    TransportAlive = false;
    return true;
  }
  return false;
}

bool AttrSetClientProc::ownsChildIdentity() const {
  std::lock_guard Guard(ReapMutex);
  return !ChildReaped;
}

bool AttrSetClientProc::alive() const {
  if (!TransportAlive)
    return false;
  if (observeChildExit())
    return false;
  return TransportAlive;
}

bool AttrSetClientProc::stop() noexcept {
  if (Input.get_id() == std::this_thread::get_id()) {
    try {
      Client.closeInbound();
    } catch (...) {
    }
    return false;
  }

  {
    std::unique_lock Lock(StopMutex);
    if (Phase == StopPhase::Stopped)
      return true;
    if (Phase == StopPhase::Stopping) {
      StopChanged.wait(Lock, [this] { return Phase == StopPhase::Stopped; });
      return true;
    }
    Phase = StopPhase::Stopping;
  }

  try {
    Client.closeInbound();
  } catch (...) {
    // Cancellation and destruction are no-throw paths. Continue terminating,
    // joining, and reaping even if a client callback misbehaves.
  }
  bool LeaderStopped = observeChildExit();
  bool Reaped = false;
  const pid_t PID = Proc.proc().PID;
  const pid_t ProcessGroup = Proc.proc().ProcessGroup;
  const bool HasDedicatedGroup =
      ProcessGroup > 0 && ProcessGroup == PID && ProcessGroup != ::getpgrp();
  auto signalWorkerTree = [&](int Signal) {
    if (!ownsChildIdentity())
      return;
    if (HasDedicatedGroup && ::kill(-ProcessGroup, Signal) == 0)
      return;
    (void)::kill(PID, Signal);
  };
  auto workerTreeAlive = [&] {
    if (!ownsChildIdentity())
      return false;
    const pid_t Target = HasDedicatedGroup ? -ProcessGroup : PID;
    errno = 0;
    return ::kill(Target, 0) == 0 || errno == EPERM;
  };

  if (!LeaderStopped || workerTreeAlive()) {
    signalWorkerTree(SIGTERM);
    const auto Deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    do {
      if (!LeaderStopped)
        LeaderStopped = observeChildExit();
      if (!workerTreeAlive())
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < Deadline);
  }
  if (workerTreeAlive())
    signalWorkerTree(SIGKILL);
  Reaped = reapChild();

  if (Input.joinable())
    Input.join();
  if (!Reaped)
    reapChild();
  TransportAlive = false;

  {
    std::lock_guard Guard(StopMutex);
    Phase = StopPhase::Stopped;
  }
  StopChanged.notify_all();
  return true;
}
