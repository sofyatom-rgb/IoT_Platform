#pragma once

#include <functional>
#include <string>

#include "telemetry/clients/NotificationServiceClient.h"
#include "telemetry/repositories/TelemetryRepository.h"

// Сервисный слой: получает данные от контроллера и просит репозиторий сохранить их.
class TelemetryService {
 public:
  explicit TelemetryService(drogon::orm::DbClientPtr db);
  TelemetryService(drogon::orm::DbClientPtr db,
                   double temperatureThreshold,
                   NotificationServiceClient notificationClient);

  void save(int deviceId,
            double temperature,
            std::function<void(const std::string &)> success,
            std::function<void(const std::string &)> error) const;

  void getByDeviceId(int deviceId,
                     std::function<void(const std::string &)> success,
                     std::function<void(const std::string &)> error) const;

  void deleteByDeviceId(int deviceId,
                        std::function<void()> success,
                        std::function<void(const std::string &)> error) const;

 private:
  drogon::orm::DbClientPtr db_;
  TelemetryRepository repository_;
  double temperatureThreshold_;
  NotificationServiceClient notificationClient_;
};
