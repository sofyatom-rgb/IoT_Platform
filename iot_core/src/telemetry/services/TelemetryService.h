#pragma once

#include <functional>
#include <string>

#include "telemetry/repositories/TelemetryRepository.h"

// Сервисный слой: получает данные от контроллера и просит репозиторий сохранить их.
class TelemetryService {
 public:
  explicit TelemetryService(drogon::orm::DbClientPtr db);

  void save(int deviceId,
            double temperature,
            std::function<void()> success,
            std::function<void(const std::string &)> error) const;

 private:
  TelemetryRepository repository_;
};
