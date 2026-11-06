#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// Выполняет блокирующие вызовы Telegram Bot API вне event loop Drogon.
class TelegramThreadPool {
 public:
  explicit TelegramThreadPool(std::size_t threadCount = 4);
  ~TelegramThreadPool();

  TelegramThreadPool(const TelegramThreadPool &) = delete;
  TelegramThreadPool &operator=(const TelegramThreadPool &) = delete;

  void post(std::function<void()> task);

 private:
  void workerLoop();

  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool stopping_ = false;
};
