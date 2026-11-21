#pragma once

#include <functional>
#include <memory>
#include <string>

#include "common/http/HttpTransport.h"
#include "telemetry/clients/INotificationServiceClient.h"

/// HTTP-клиент сервиса уведомлений.
class NotificationServiceClient : public INotificationServiceClient {
 public:
  /// Создаёт клиент с Drogon-транспортом для указанного URL.
  explicit NotificationServiceClient(std::string baseUrl);

  /// Создаёт клиент с внедрённым HTTP-транспортом.
  explicit NotificationServiceClient(
      std::shared_ptr<iot::http::IHttpTransport> transport);

  /// Создаёт клиент с URL и тестовым или пользовательским транспортом.
  NotificationServiceClient(
      std::string baseUrl,
      std::shared_ptr<iot::http::IHttpTransport> transport);

  /// Отправляет уведомление о превышении температурного порога.
  void sendTemperatureAlert(
      const std::string &chatId, const std::string &deviceName,
      double temperature, double threshold, std::function<void()> success,
      std::function<void(const std::string &)> error) const override;

 private:
  std::shared_ptr<iot::http::IHttpTransport> transport_;
  bool configured_{true};
};
