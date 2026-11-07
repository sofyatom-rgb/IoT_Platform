#pragma once

#include <tgbot/Bot.h>

#include <functional>
#include <memory>
#include <string>

#include "clients/WorkerThreadPool.h"

class TelegramBotClient {
 public:
  TelegramBotClient(std::string botToken,
                    std::shared_ptr<WorkerThreadPool> threadPool);

  void sendMessage(const std::string &chatId,
                   const std::string &text,
                   std::function<void()> success,
                   std::function<void(const std::string &)> error) const;

  // Токен может быть пустым (канал отключён) — тогда bot_ == nullptr.
  bool isEnabled() const { return bot_ != nullptr; }

 private:
  std::shared_ptr<TgBot::Bot> bot_;
  std::shared_ptr<WorkerThreadPool> threadPool_;
};
