#pragma once

#include "nixd/Protocol/AttrSet.h"
#include "nixd/Support/StreamProc.h"

#include <lspserver/LSPServer.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace nixd {

class AttrSetClient : public lspserver::LSPServer {

  llvm::unique_function<void(const EvalExprParams &Params,
                             lspserver::Callback<EvalExprResponse> Reply)>
      EvalExpr;

  llvm::unique_function<void(const AttrPathInfoParams &Params,
                             lspserver::Callback<AttrPathInfoResponse> Reply)>
      AttrPathInfo;

  llvm::unique_function<void(
      const AttrPathCompleteParams &Params,
      lspserver::Callback<AttrPathCompleteResponse> Reply)>
      AttrPathComplete;

  llvm::unique_function<void(const AttrPathInfoParams &Params,
                             lspserver::Callback<OptionInfoResponse> Reply)>
      OptionInfo;

  llvm::unique_function<void(const AttrPathCompleteParams &Params,
                             lspserver::Callback<OptionCompleteResponse> Reply)>
      OptionComplete;

  llvm::unique_function<void(std::nullptr_t)> Exit;

public:
  AttrSetClient(std::unique_ptr<lspserver::InboundPort> In,
                std::unique_ptr<lspserver::OutboundPort> Out);

  /// \brief Request eval some expression.
  /// The expression should be evaluted to attrset.
  void evalExpr(const EvalExprParams &Params,
                lspserver::Callback<EvalExprResponse> Reply) {
    return EvalExpr(Params, std::move(Reply));
  }

  void attrpathInfo(const AttrPathInfoParams &Params,
                    lspserver::Callback<AttrPathInfoResponse> Reply) {
    AttrPathInfo(Params, std::move(Reply));
  }

  void attrpathComplete(const AttrPathCompleteParams &Params,
                        lspserver::Callback<AttrPathCompleteResponse> Reply) {
    AttrPathComplete(Params, std::move(Reply));
  }

  void optionInfo(const AttrPathInfoParams &Params,
                  lspserver::Callback<OptionInfoResponse> Reply) {
    OptionInfo(Params, std::move(Reply));
  }

  void optionComplete(const AttrPathCompleteParams &Params,
                      lspserver::Callback<OptionCompleteResponse> Reply) {
    OptionComplete(Params, std::move(Reply));
  }

  void exit() { Exit(nullptr); }

  /// Get executable path for launching the server.
  /// \returns null terminated string.
  static const char *getExe();
};

class AttrSetClientProc {
  StreamProc Proc;
  AttrSetClient Client;
  std::function<void()> OnDeath;
  std::thread Input;
  mutable std::atomic<bool> TransportAlive{true};
  enum class StopPhase { Running, Stopping, Stopped };
  std::mutex StopMutex;
  std::condition_variable StopChanged;
  StopPhase Phase = StopPhase::Running;
  mutable std::mutex ReapMutex;
  mutable bool LeaderExited = false;
  mutable bool ChildReaped = false;

  bool observeChildExit() const;
  bool ownsChildIdentity() const;
  bool reapChild() const;

public:
  /// \brief Check if the process is still alive
  /// \returns nullptr if it has been dead.
  AttrSetClient *client();
  [[nodiscard]] bool alive() const;
  [[nodiscard]] pid_t pid() const { return Proc.proc().PID; }
  /// Stop, join, and reap the worker. Concurrent owning-thread callers wait
  /// for the single Running -> Stopping -> Stopped transition. Returns false
  /// on the input thread, where joining or final destruction is forbidden.
  bool stop() noexcept;
  ~AttrSetClientProc();

  /// \see StreamProc::StreamProc
  /// OnDeath runs on the input thread and must retain only weak ownership. It
  /// must never release the final AttrSetClientProc owner on that thread.
  AttrSetClientProc(const std::function<int()> &Action,
                    std::function<void()> OnDeath = {});
};

} // namespace nixd
