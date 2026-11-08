#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>

// Клиент уведомлений: отправляет HTTP-запрос в notification_service.
class NotificationClient {
 public:
  explicit NotificationClient(std::string baseUrl);

  void sendTemperatureAlert(
      const std::string& chatId,
      const std::string& deviceName,
      double temperature,
      double threshold,
      std::function<void()> success,
      std::function<void(const std::string&)> error) const;

 private:
  std::string baseUrl_;
};