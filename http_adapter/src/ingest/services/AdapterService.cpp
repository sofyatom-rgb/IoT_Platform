#include "ingest/services/AdapterService.h"

#include <stdexcept>
#include <utility>

AdapterService::AdapterService(std::shared_ptr<IIoTClient> client)
    : client_(std::move(client)) {
  if (!client_) throw std::invalid_argument("AdapterService requires an IoT client");
}

void AdapterService::processTelemetry(
    int deviceId, double temperature,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  client_->sendTelemetry(deviceId, temperature, std::move(success),
                         std::move(error));
}

void AdapterService::getTelemetry(
    int deviceId, std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  client_->getTelemetry(deviceId, std::move(success), std::move(error));
}

void AdapterService::deleteTelemetry(
    int deviceId, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  client_->deleteTelemetry(deviceId, std::move(success), std::move(error));
}
