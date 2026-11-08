#pragma once

#include <functional>
#include <string>

class IRuleEngineClient {
 public:
  virtual ~IRuleEngineClient() = default;

  virtual void checkTelemetry(
      int deviceId, double temperature,
      std::function<void(const std::string &)> success,
      std::function<void(const std::string &)> error) const = 0;
};
