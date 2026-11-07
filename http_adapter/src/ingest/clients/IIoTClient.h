#pragma once

#include <functional>
#include <string>

// Порт для взаимодействия Adapter с IoT Core.
class IIoTClient {
 public:
  virtual ~IIoTClient() = default;
  virtual void sendTelemetry(int deviceId, double temperature,
                             std::function<void()> success,
                             std::function<void(const std::string &)> error) const = 0;
  virtual void getTelemetry(int deviceId,
                            std::function<void(const std::string &)> success,
                            std::function<void(const std::string &)> error) const = 0;
  virtual void deleteTelemetry(int deviceId, std::function<void()> success,
                                std::function<void(const std::string &)> error) const = 0;
};
