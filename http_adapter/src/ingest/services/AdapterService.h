#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ingest/clients/IIoTClient.h"

// Сервисный слой адаптера. Клиент IoT Core внедряется через интерфейс,
// чтобы бизнес-сценарий можно было проверить без HTTP и запущенного Core.
class AdapterService {
 public:
  explicit AdapterService(std::shared_ptr<IIoTClient> client);

  void processTelemetry(int deviceId, double temperature,
                        std::function<void()> success,
                        std::function<void(const std::string &)> error) const;
  void getTelemetry(int deviceId,
                    std::function<void(const std::string &)> success,
                    std::function<void(const std::string &)> error) const;
  void deleteTelemetry(int deviceId, std::function<void()> success,
                       std::function<void(const std::string &)> error) const;

 private:
  std::shared_ptr<IIoTClient> client_;
};
