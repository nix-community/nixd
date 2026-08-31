#include "nixd-config.h"

#include "nixd/Eval/AttrSetClient.h"

#include <array>
#include <cerrno>

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
                                     std::function<void()> OnDeath,
                                     std::span<const int> ChildFDs)
    : Proc(Action, ChildFDs), Identity(std::make_shared<ProcessTreeIdentity>(
                                  Proc.proc().PID, Proc.proc().ProcessGroup)),
      Client(Proc.mkIn(), Proc.mkOut()), OnDeath(std::move(OnDeath)),
      Input([this] { runInput(); }) {
  Proc.claimProcess();
}

AttrSetClientProc::AttrSetClientProc(const ExecSpec &Spec,
                                     std::function<void()> OnDeath)
    : Proc(Spec), Identity(std::make_shared<ProcessTreeIdentity>(
                      Proc.proc().PID, Proc.proc().ProcessGroup)),
      Client(Proc.mkIn(), Proc.mkOut()), OnDeath(std::move(OnDeath)),
      Input([this] { runInput(); }) {
  Proc.claimProcess();
}

void AttrSetClientProc::runInput() {
  Client.run();
  TransportAlive = false;
  if (!OnDeath)
    return;
  try {
    OnDeath();
  } catch (...) {
    // A process-input callback must never escape the input thread.
  }
}

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
  if (!Identity->ownsIdentity())
    return true;

  int Status = 0;
  const pid_t Result = Identity->reapChild(Status, 0);

  if (Result == Proc.proc().PID || (Result < 0 && errno == ECHILD)) {
    TransportAlive = false;
    return true;
  }
  return false;
}

bool AttrSetClientProc::observeChildExit() const {
  if (Identity->observeLeaderExit()) {
    TransportAlive = false;
    return true;
  }
  return false;
}

bool AttrSetClientProc::alive() const {
  if (!TransportAlive)
    return false;
  if (observeChildExit())
    return false;
  return TransportAlive;
}

std::shared_ptr<ProcessTreeIdentity> AttrSetClientProc::prepareStop() noexcept {
  if (Input.get_id() == std::this_thread::get_id()) {
    try {
      Client.closeInbound();
    } catch (...) {
    }
    return {};
  }

  {
    std::unique_lock Lock(StopMutex);
    if (Phase == StopPhase::Stopped)
      return {};
    if (Phase == StopPhase::Stopping) {
      StopChanged.wait(Lock, [this] { return Phase == StopPhase::Stopped; });
      return {};
    }
    Phase = StopPhase::Stopping;
  }

  try {
    Client.closeInbound();
  } catch (...) {
    // Cancellation and destruction are no-throw paths. The transition owner
    // must still terminate, join, and reap the process.
  }
  return Identity;
}

void AttrSetClientProc::finishStop() noexcept {
  bool Reaped = reapChild();
  if (Input.joinable())
    Input.join();
  if (!Reaped)
    (void)reapChild();
  TransportAlive = false;

  {
    std::lock_guard Guard(StopMutex);
    Phase = StopPhase::Stopped;
  }
  StopChanged.notify_all();
}

bool AttrSetClientProc::stop() noexcept {
  if (Input.get_id() == std::this_thread::get_id()) {
    try {
      Client.closeInbound();
    } catch (...) {
    }
    return false;
  }

  auto StopIdentity = prepareStop();
  if (!StopIdentity)
    return true;
  const std::array Trees{StopIdentity};
  cancelProcessTrees(Trees);
  finishStop();
  return true;
}
