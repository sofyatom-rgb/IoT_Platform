#pragma once

#include <functional>
#include <memory>
#include <string>

#include "clients/ITelegramBotClient.h"

/// Формирует сообщения о превышении температуры и отправляет их в Telegram.
class NotificationService {
 public:
  /// Создаёт сервис с внедрённым Telegram-клиентом.
  explicit NotificationService(
      std::shared_ptr<ITelegramBotClient> telegramBotClient);

  /// Отправляет предупреждение о превышении температурного порога.
  void sendTemperatureAlert(
      const std::string &chatId, const std::string &deviceName,
      double temperature, double threshold, std::function<void()> success,
      std::function<void(const std::string &)> error) const;

 private:
  std::shared_ptr<ITelegramBotClient> telegramBotClient_;
};
