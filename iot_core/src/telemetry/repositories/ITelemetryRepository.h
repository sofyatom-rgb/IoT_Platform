#pragma once

#include <functional>
#include <string>

class ITelemetryRepository {
 public:
  virtual ~ITelemetryRepository() = default;
  virtual void save(int deviceId, double temperature,
                    std::function<void()> success,
                    std::function<void(const std::string &)> error) const = 0;
  virtual void getByDeviceId(int deviceId,
                             std::function<void(const std::string &)> success,
                             std::function<void(const std::string &)> error) const = 0;
  virtual void deleteByDeviceId(int deviceId, std::function<void()> success,
                                std::function<void(const std::string &)> error) const = 0;
};
