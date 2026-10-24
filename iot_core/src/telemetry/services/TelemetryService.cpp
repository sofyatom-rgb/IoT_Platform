#include "telemetry/services/TelemetryService.h"

#include <utility>

TelemetryService::TelemetryService(drogon::orm::DbClientPtr db)
    : repository_(std::move(db)) {}

void TelemetryService::save(
    int deviceId,
    double temperature,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  // Пока сервис не меняет данные, только передаёт их репозиторию.
  repository_.save(deviceId, temperature, std::move(success), std::move(error));
}

void TelemetryService::getByDeviceId(
    int deviceId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  repository_.getByDeviceId(deviceId, std::move(success), std::move(error));
}
