#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// Общий пул потоков: выполняет блокирующие вызовы (Telegram, email)
// вне event loop Drogon.
class WorkerThreadPool {
 public:
  explicit WorkerThreadPool(std::size_t threadCount = 4);
  ~WorkerThreadPool();

  WorkerThreadPool(const WorkerThreadPool &) = delete;
  WorkerThreadPool &operator=(const WorkerThreadPool &) = delete;

  void post(std::function<void()> task);

 private:
  void workerLoop();

  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool stopping_ = false;
};
