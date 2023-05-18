#include "nixd/Scheduler.h"

namespace nixd {

void EvaluationTask::run() noexcept {
  try {
    IValue->toValue(*State);
    Action(nullptr);
  } catch (nix::Error &Err) {
    Action(&Err);
  } catch (...) {
    auto Uncaught = std::current_exception();
    // should panic here
    std::rethrow_exception(Uncaught);
  }
}

void Worker::assign(std::unique_ptr<Task> TheTask) {
  State = Working;
  TaskFuture = std::async(std::launch::async,
                          [Task = std::move(TheTask), this]() mutable {
                            Task->run();
                            State = IDLE;
                          });
}

void Scheduler::dispatchJobs() {
  for (auto &Worker : Workers) {
    if (Tasks.empty())
      break;
    // TODO: check cancellation
    if (Worker.isIDLE()) {
      // Consume front, assign it to this worker
      Worker.assign(std::move(Tasks.front()));
      Tasks.pop_front();
    }
  }
}

} // namespace nixd
