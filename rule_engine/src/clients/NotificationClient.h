#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>

/// Отправляет уведомления из rule_engine в notification_service.
class NotificationClient {
 public:
  /// Создаёт клиент для указанного адреса сервиса уведомлений.
  explicit NotificationClient(std::string baseUrl);

  /// Отправляет предупреждение о нарушении температурного правила.
  void sendTemperatureAlert(
      const std::string &chatId, const std::string &deviceName,
      double temperature, double threshold, std::function<void()> success,
      std::function<void(const std::string &)> error) const;

 private:
  std::string baseUrl_;
};
