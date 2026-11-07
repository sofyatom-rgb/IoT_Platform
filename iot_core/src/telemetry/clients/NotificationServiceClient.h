#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>

struct ChannelStatuses {
  std::string telegram;
  std::string email;
};

class NotificationServiceClient {
 public:
  explicit NotificationServiceClient(std::string baseUrl);

  // success получает итоговые статусы каналов ("sent"/"skipped"/"failed: ...").
  void sendTemperatureAlert(
      const std::string &chatId,
      const std::string &email,
      const std::string &deviceName,
      double temperature,
      double threshold,
      std::function<void(const ChannelStatuses &)> success,
      std::function<void(const std::string &)> error) const;

 private:
  std::string baseUrl_;
};
