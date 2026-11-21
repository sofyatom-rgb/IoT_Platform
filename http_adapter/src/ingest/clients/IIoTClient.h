#pragma once

#include <functional>
#include <string>

/// Контракт запросов от адаптера к IoT Core.
class IIoTClient {
 public:
  /// Позволяет удалять реализацию через указатель на интерфейс.
  virtual ~IIoTClient() = default;

  /// Передаёт новое показание температуры в IoT Core.
  virtual void sendTelemetry(int deviceId, double temperature,
                             std::function<void(const std::string &)> success,
                             std::function<void(const std::string &)> error) const = 0;

  /// Запрашивает телеметрию устройства.
  virtual void getTelemetry(int deviceId,
                            std::function<void(const std::string &)> success,
                            std::function<void(const std::string &)> error) const = 0;

  /// Удаляет телеметрию устройства.
  virtual void deleteTelemetry(int deviceId, std::function<void()> success,
                               std::function<void(const std::string &)> error) const = 0;
};
