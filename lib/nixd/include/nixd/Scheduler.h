#pragma once

#include "lspserver/Function.h"

#include <cstdint>
#include <memory>
#include <nix/error.hh>
#include <nix/installables.hh>
#include <nix/nixexpr.hh>

#include <llvm/ADT/FunctionExtras.h>

#include <deque>
#include <exception>
#include <future>
#include <optional>
#include <utility>

namespace nixd {

class Task {
public:
  /// The internal name of this task.
  std::string Name;

  Task() : Task("") {}
  Task(std::string Name) : Name(std::move(Name)) {}

  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;
  Task(Task &&) = default;
  Task &operator=(Task &&) = default;

  virtual ~Task() = default;

  virtual void run() noexcept = 0;
};

class EvaluationTask : public Task {

public:
  using CallbackAction =
      llvm::unique_function<void(std::optional<std::exception *>)>;

private:
  CallbackAction Action;

  nix::InstallableValue *IValue;

  std::unique_ptr<nix::EvalState> State;

public:
  EvaluationTask(std::string Name, CallbackAction Action,
                 nix::InstallableValue *IValue,
                 std::unique_ptr<nix::EvalState> State)
      : Task(std::move(Name)), Action(std::move(Action)), IValue(IValue),
        State(std::move(State)) {}

  void run() noexcept override;
};

class Worker {
private:
  enum WorkerState { IDLE, Working } State = IDLE;

  std::unique_ptr<Task> TheTask;

  std::future<void> TaskFuture;

public:
  bool isIDLE() { return State == IDLE; }
  void assign(std::unique_ptr<Task> TheTask);
  void wait() {
    if (State == Working)
      TaskFuture.wait();
  }
};

class Scheduler {

  std::deque<std::unique_ptr<Task>> Tasks;

  std::vector<Worker> Workers;

public:
  Scheduler(uint32_t NumWorkers) {
    Workers.clear();
    for (uint32_t _ = 0; _ < NumWorkers; _++) {
      Workers.emplace_back(Worker{});
    }
  }
  /// Schedule a task, after finished (canceled, or completed), the callback
  /// function will be called. Nix may throw exceptions in this case, the
  /// exception it self will be handled in this function, and pass the result
  /// into the callback function.
  void schedule(std::unique_ptr<Task> Task) {
    Tasks.push_back(std::move(Task));
    dispatchJobs();
  }

  void wait() {
    for (auto &Worker : Workers)
      Worker.wait();
  }

  void dispatchJobs();
};

} // namespace nixd
