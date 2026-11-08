#include "telemetry/services/TelemetryService.h"

#include <stdexcept>
#include <utility>

TelemetryService::TelemetryService(
    std::shared_ptr<ITelemetryRepository> repository)
    : repository_(std::move(repository)) {
  if (!repository_) {
    throw std::invalid_argument("TelemetryService requires a repository");
  }
}

TelemetryService::TelemetryService(
    std::shared_ptr<ITelemetryRepository> repository,
    std::shared_ptr<IRuleEngineClient> ruleEngineClient)
    : repository_(std::move(repository)),
      ruleEngineClient_(std::move(ruleEngineClient)) {
  if (!repository_ || !ruleEngineClient_) {
    throw std::invalid_argument("TelemetryService requires repository and Rule Engine client");
  }
}

void TelemetryService::save(
    int deviceId, double temperature,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  if (!ruleEngineClient_) {
    if (error) error("TelemetryService Rule Engine client is not configured");
    return;
  }

  const auto ruleEngineClient = ruleEngineClient_;
  const auto ruleEngineSuccess = success;
  const auto ruleEngineError = error;
  repository_->save(
      deviceId, temperature,
      [ruleEngineClient, deviceId, temperature, success = ruleEngineSuccess,
       error = ruleEngineError]() mutable {
        ruleEngineClient->checkTelemetry(deviceId, temperature, success, error);
      },
      std::move(error));
}

void TelemetryService::getByDeviceId(
    int deviceId, std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  repository_->getByDeviceId(deviceId, std::move(success), std::move(error));
}

void TelemetryService::deleteByDeviceId(
    int deviceId, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  repository_->deleteByDeviceId(deviceId, std::move(success), std::move(error));
}
