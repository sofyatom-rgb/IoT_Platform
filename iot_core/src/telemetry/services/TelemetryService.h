#pragma once

#include <functional>
#include <memory>
#include <string>

#include "telemetry/clients/IRuleEngineClient.h"
#include "telemetry/repositories/ITelemetryRepository.h"

// Saves telemetry, then asks Rule Engine to evaluate the reading.
class TelemetryService {
 public:
  // For read/delete operations that do not need Rule Engine.
  explicit TelemetryService(std::shared_ptr<ITelemetryRepository> repository);

  TelemetryService(std::shared_ptr<ITelemetryRepository> repository,
                   std::shared_ptr<IRuleEngineClient> ruleEngineClient);

  void save(int deviceId, double temperature,
            std::function<void(const std::string &)> success,
            std::function<void(const std::string &)> error) const;
  void getByDeviceId(int deviceId,
                     std::function<void(const std::string &)> success,
                     std::function<void(const std::string &)> error) const;
  void deleteByDeviceId(int deviceId, std::function<void()> success,
                        std::function<void(const std::string &)> error) const;

 private:
  std::shared_ptr<ITelemetryRepository> repository_;
  std::shared_ptr<IRuleEngineClient> ruleEngineClient_;
};
