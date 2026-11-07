#include "clients/WorkerThreadPool.h"

#include <utility>

WorkerThreadPool::WorkerThreadPool(std::size_t threadCount) {
  if (threadCount == 0) {
    threadCount = 1;
  }
  workers_.reserve(threadCount);
  for (std::size_t i = 0; i < threadCount; ++i) {
    workers_.emplace_back([this]() { workerLoop(); });
  }
}

WorkerThreadPool::~WorkerThreadPool() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  condition_.notify_all();
  for (auto &worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

void WorkerThreadPool::post(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) {
      return;
    }
    tasks_.push(std::move(task));
  }
  condition_.notify_one();
}

void WorkerThreadPool::workerLoop() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      condition_.wait(lock,
                      [this]() { return stopping_ || !tasks_.empty(); });
      if (stopping_ && tasks_.empty()) {
        return;
      }
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    task();
  }
}
