#pragma once

#include <functional>
#include <optional>
#include <string>

struct TelemetryQuery {
  std::optional<std::string> from;
  std::optional<std::string> to;
  std::optional<int> afterId;
  bool allTime{false};
};

class ITelemetryRepository {
 public:
  virtual ~ITelemetryRepository() = default;
  virtual void save(int deviceId, double temperature,
                    std::function<void()> success,
                    std::function<void(const std::string &)> error) const = 0;
  virtual void getByDeviceId(int deviceId,
                             std::function<void(const std::string &)> success,
                             std::function<void(const std::string &)> error) const = 0;
  virtual void queryByDeviceId(
      int deviceId, const TelemetryQuery &,
      std::function<void(const std::string &)> success,
      std::function<void(const std::string &)> error) const {
    getByDeviceId(deviceId, success, error);
  }
  virtual void deleteByDeviceId(int deviceId, std::function<void()> success,
                                std::function<void(const std::string &)> error) const = 0;
};
