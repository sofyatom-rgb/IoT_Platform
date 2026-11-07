#pragma once

#include <functional>
#include <string>

class INotificationServiceClient {
 public:
  virtual ~INotificationServiceClient() = default;
  virtual void sendTemperatureAlert(
      const std::string &chatId, const std::string &deviceName,
      double temperature, double threshold, std::function<void()> success,
      std::function<void(const std::string &)> error) const = 0;
};
