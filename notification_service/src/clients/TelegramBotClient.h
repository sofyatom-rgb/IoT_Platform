#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>

class TelegramBotClient {
 public:
  explicit TelegramBotClient(std::string botToken);

  void sendMessage(const std::string &chatId,
                   const std::string &text,
                   std::function<void()> success,
                   std::function<void(const std::string &)> error) const;

 private:
  std::string botToken_;
};
