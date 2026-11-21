#pragma once

#include <functional>
#include <string>

#include "ingest/clients/IoTClient.h"

// Сервисный слой адаптера: валидирует данные и передает их клиенту.
class AdapterService {
 public:
  explicit AdapterService(const std::string &iotCoreUrl);

  void processTelemetry(int deviceId,
                        double temperature,
                        std::function<void()> success,
                        std::function<void(const std::string &)> error) const;

  void getTelemetry(int deviceId,
                    std::function<void(const std::string &)> success,
                    std::function<void(const std::string &)> error) const;

  void deleteTelemetry(int deviceId,
                       std::function<void()> success,
                       std::function<void(const std::string &)> error) const;

 private:
  IoTClient client_;
};