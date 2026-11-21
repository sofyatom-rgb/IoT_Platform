#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ingest/clients/IIoTClient.h"

/// Передаёт запросы адаптера в IoT Core.
class AdapterService {
 public:
  /// Создаёт сервис, настроенный на указанный IoT Core URL.
  explicit AdapterService(const std::string &iotCoreUrl);

  /// Создаёт сервис с внедрённым клиентом.
  explicit AdapterService(std::shared_ptr<IIoTClient> client);

  /// Передаёт новое показание температуры в IoT Core.
  void processTelemetry(int deviceId, double temperature,
                        std::function<void(const std::string &)> success,
                        std::function<void(const std::string &)> error) const;

  void processTelemetry(int deviceId, double temperature,
                        std::function<void()> success,
                        std::function<void(const std::string &)> error) const;

  /// Возвращает телеметрию устройства.
  void getTelemetry(int deviceId,
                    std::function<void(const std::string &)> success,
                    std::function<void(const std::string &)> error) const;

  /// Удаляет телеметрию устройства.
  void deleteTelemetry(int deviceId, std::function<void()> success,
                       std::function<void(const std::string &)> error) const;

 private:
  std::shared_ptr<IIoTClient> client_;
};
