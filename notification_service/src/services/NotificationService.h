#pragma once

#include <functional>
#include <string>

#include "clients/TelegramBotClient.h"

class NotificationService {
 public:
  explicit NotificationService(TelegramBotClient telegramBotClient);

  void sendTemperatureAlert(
      const std::string &chatId,
      const std::string &deviceName,
      double temperature,
      double threshold,
      std::function<void()> success,
      std::function<void(const std::string &)> error) const;

 private:
  TelegramBotClient telegramBotClient_;
};
