#pragma once

#include <functional>
#include <string>
#include <utility>

struct NotificationChannelStatuses {
  std::string telegram = "skipped";
  std::string email = "skipped";
};

class INotificationServiceClient {
 public:
  virtual ~INotificationServiceClient() = default;
  virtual void sendTemperatureAlert(
      const std::string &chatId, const std::string &deviceName,
      double temperature, double threshold, std::function<void()> success,
      std::function<void(const std::string &)> error) const = 0;
  virtual void sendTemperatureAlert(
      int deviceId, const std::string &chatId, const std::string &email,
      const std::string &deviceName, double temperature, double threshold,
      std::function<void(const NotificationChannelStatuses &)> success,
      std::function<void(const std::string &)> error) const {
    (void)deviceId;
    (void)email;
    sendTemperatureAlert(
        chatId, deviceName, temperature, threshold,
        [success = std::move(success)] {
          success(NotificationChannelStatuses{"sent", "skipped"});
        },
        std::move(error));
  }
  virtual void clearTemperatureAlert(
      int, std::function<void()> success,
      std::function<void(const std::string &)> error) const {
    (void)error;
    if (success) success();
  }
};
