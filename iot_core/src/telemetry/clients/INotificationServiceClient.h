#pragma once

#include <functional>
#include <string>

/// Контракт отправки уведомлений о превышении температуры.
class INotificationServiceClient {
 public:
  /// Позволяет удалять реализацию через указатель на интерфейс.
  virtual ~INotificationServiceClient() = default;

  /// Отправляет уведомление о превышении температурного порога.
  virtual void sendTemperatureAlert(
      const std::string &chatId, const std::string &deviceName,
      double temperature, double threshold, std::function<void()> success,
      std::function<void(const std::string &)> error) const = 0;
};
