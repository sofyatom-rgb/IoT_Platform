#pragma once

#include <functional>
#include <memory>
#include <string>

#include "clients/ITelegramBotClient.h"

class NotificationService {
 public:
  explicit NotificationService(std::shared_ptr<ITelegramBotClient> telegramBotClient);

  void sendTemperatureAlert(
      const std::string &chatId, const std::string &deviceName,
      double temperature, double threshold, std::function<void()> success,
      std::function<void(const std::string &)> error) const;

 private:
  std::shared_ptr<ITelegramBotClient> telegramBotClient_;
};
