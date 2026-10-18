#pragma once

#include <drogon/drogon.h>
#include <functional>
#include <string>

// Клиент: отвечает за отправку HTTP-запросов во внешний сервис (iot_core).
class IoTClient {
 public:
  explicit IoTClient(const std::string &baseUrl);

  void sendTelemetry(int deviceId,
                     double temperature,
                     std::function<void()> success,
                     std::function<void(const std::string &)> error) const;

 private:
  std::string baseUrl_;
};