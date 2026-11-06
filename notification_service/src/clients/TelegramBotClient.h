#pragma once

#include <tgbot/Bot.h>

#include <functional>
#include <memory>
#include <string>

#include "clients/TelegramThreadPool.h"

class TelegramBotClient {
 public:
  TelegramBotClient(std::string botToken,
                    std::shared_ptr<TelegramThreadPool> threadPool);

  void sendMessage(const std::string &chatId,
                   const std::string &text,
                   std::function<void()> success,
                   std::function<void(const std::string &)> error) const;

 private:
  std::shared_ptr<TgBot::Bot> bot_;
  std::shared_ptr<TelegramThreadPool> threadPool_;
};
