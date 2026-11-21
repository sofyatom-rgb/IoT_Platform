#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>

class NotificationServiceClient {
 public:
  explicit NotificationServiceClient(std::string baseUrl);

  void sendTemperatureAlert(
      const std::string &chatId,
      const std::string &deviceName,
      double temperature,
      double threshold,
      std::function<void()> success,
      std::function<void(const std::string &)> error) const;

 private:
  std::string baseUrl_;
};
