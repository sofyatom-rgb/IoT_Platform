#pragma once

#include <functional>
#include <memory>
#include <string>

#include "common/http/HttpTransport.h"
#include "ingest/clients/IIoTClient.h"

/// HTTP-клиент для передачи телеметрии в IoT Core.
class IoTClient : public IIoTClient {
 public:
  /// Создаёт клиент с реальным Drogon-транспортом для указанного URL.
  explicit IoTClient(const std::string &baseUrl);

  /// Создаёт клиент с внедрённым HTTP-транспортом.
  explicit IoTClient(std::shared_ptr<iot::http::IHttpTransport> transport);

  /// Отправляет новое показание температуры.
  void sendTelemetry(int deviceId, double temperature,
                     std::function<void()> success,
                     std::function<void(const std::string &)> error) const override;

  /// Запрашивает телеметрию устройства.
  void getTelemetry(int deviceId,
                    std::function<void(const std::string &)> success,
                    std::function<void(const std::string &)> error) const override;

  /// Удаляет телеметрию устройства.
  void deleteTelemetry(int deviceId, std::function<void()> success,
                       std::function<void(const std::string &)> error) const override;

 private:
  std::shared_ptr<iot::http::IHttpTransport> transport_;
};
