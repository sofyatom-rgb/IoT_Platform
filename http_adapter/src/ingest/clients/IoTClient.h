#pragma once

#include <functional>
#include <memory>
#include <string>

#include "common/http/HttpTransport.h"
#include "ingest/clients/IIoTClient.h"

// Клиент IoT Core. Транспорт внедряется снаружи, чтобы HTTP-вызовы
// можно было проверять без реальной сети и запущенного сервиса.
class IoTClient : public IIoTClient {
 public:
  explicit IoTClient(std::shared_ptr<iot::http::IHttpTransport> transport);

  void sendTelemetry(int deviceId,
                     double temperature,
                     std::function<void(const std::string &)> success,
                     std::function<void(const std::string &)> error) const override;

  void getTelemetry(int deviceId,
                    std::function<void(const std::string &)> success,
                    std::function<void(const std::string &)> error) const override;

  void deleteTelemetry(int deviceId,
                       std::function<void()> success,
                       std::function<void(const std::string &)> error) const override;

 private:
  std::shared_ptr<iot::http::IHttpTransport> transport_;
};
