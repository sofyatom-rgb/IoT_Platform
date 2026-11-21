#include "ingest/services/AdapterService.h"

#include <utility>

AdapterService::AdapterService(const std::string &iotCoreUrl)
    : client_(iotCoreUrl) {}

void AdapterService::processTelemetry(
    int deviceId,
    double temperature,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  
  // Здесь можно добавить бизнес-логику (например, обогащение данных или ретраи)
  // Пока просто передаем данные клиенту.
  client_.sendTelemetry(deviceId, temperature, std::move(success), std::move(error));
}

void AdapterService::getTelemetry(
    int deviceId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  client_.getTelemetry(deviceId, std::move(success), std::move(error));
}

void AdapterService::deleteTelemetry(
    int deviceId,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  client_.deleteTelemetry(deviceId, std::move(success), std::move(error));
}