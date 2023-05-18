#include <gtest/gtest.h>

#include "nixd/Scheduler.h"

#include <atomic>

namespace nixd {

TEST(Scheduler, Simple) {
  constexpr int NumWorkers = 20;
  constexpr int ExpectedSum = 50;
  std::atomic_int Sum;
  struct SimpleTask : Task {
    std::atomic_int &MyOp;
    SimpleTask(std::atomic_int &MyOp) : MyOp(MyOp) {}
    void run() noexcept override { MyOp++; }
  };
  Scheduler MyScheduler(NumWorkers);
  for (int Idx = 0; Idx < ExpectedSum; Idx++)
    MyScheduler.schedule(std::make_unique<SimpleTask>(Sum));
  MyScheduler.wait();
  ASSERT_EQ(Sum, ExpectedSum);
}

} // namespace nixd
