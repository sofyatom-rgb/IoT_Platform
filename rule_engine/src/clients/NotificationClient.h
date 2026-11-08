#pragma once

#include <functional>
#include <memory>
#include <string>

#include "common/http/HttpTransport.h"

// Клиент уведомлений: отправляет HTTP-запрос в notification_service.
class NotificationClient {
 public:
  NotificationClient(std::string baseUrl,
                     std::shared_ptr<iot::http::IHttpTransport> transport);

  void sendTemperatureAlert(
      int deviceId,
      const std::string& chatId,
      const std::string& email,
      const std::string& deviceName,
      double temperature,
      double threshold,
      std::function<void(const std::string&)> success,
      std::function<void(const std::string&)> error) const;

  void clearTemperatureAlert(
      int deviceId, std::function<void()> success,
      std::function<void(const std::string&)> error) const;

 private:
  std::string baseUrl_;
  std::shared_ptr<iot::http::IHttpTransport> transport_;
};