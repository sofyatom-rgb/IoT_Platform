#pragma once

#include <functional>
#include <memory>
#include <string>

#include "common/http/HttpTransport.h"
#include "telemetry/clients/INotificationServiceClient.h"

class NotificationServiceClient : public INotificationServiceClient {
 public:
  NotificationServiceClient(
      std::string baseUrl,
      std::shared_ptr<iot::http::IHttpTransport> transport);

  void sendTemperatureAlert(
      const std::string &chatId,
      const std::string &deviceName,
      double temperature,
      double threshold,
      std::function<void()> success,
      std::function<void(const std::string &)> error) const override;

 private:
  std::string baseUrl_;
  std::shared_ptr<iot::http::IHttpTransport> transport_;
};
