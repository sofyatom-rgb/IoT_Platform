#pragma once

#include <functional>
#include <memory>
#include <string>

#include "devices/services/IDevicesService.h"
#include "telemetry/clients/INotificationServiceClient.h"
#include "telemetry/repositories/ITelemetryRepository.h"
#include "users/services/IUsersService.h"

namespace drogon::orm { class DbClient; }
class NotificationServiceClient;

/// Сохраняет телеметрию и запускает уведомление при превышении порога.
class TelemetryService {
 public:
  /// Создаёт сервис с настройками уведомлений по умолчанию.
  explicit TelemetryService(std::shared_ptr<drogon::orm::DbClient> db);

  /// Создаёт сервис с PostgreSQL и заданным порогом температуры.
  TelemetryService(std::shared_ptr<drogon::orm::DbClient> db, double temperatureThreshold,
                   NotificationServiceClient notificationClient);

  /// Создаёт сервис только для операций с репозиторием.
  explicit TelemetryService(std::shared_ptr<ITelemetryRepository> repository);

  /// Создаёт сервис с внедрёнными зависимостями для уведомлений.
  TelemetryService(std::shared_ptr<ITelemetryRepository> repository,
                   std::shared_ptr<IDevicesService> devicesService,
                   std::shared_ptr<IUsersService> usersService,
                   std::shared_ptr<INotificationServiceClient> notificationClient,
                   double temperatureThreshold);

  /// Сохраняет показание и при необходимости отправляет уведомление.
  void save(int deviceId, double temperature,
            std::function<void(const std::string &)> success,
            std::function<void(const std::string &)> error) const;

  /// Возвращает последние показания устройства.
  void getByDeviceId(int deviceId,
                     std::function<void(const std::string &)> success,
                     std::function<void(const std::string &)> error) const;

  /// Удаляет историю телеметрии устройства.
  void deleteByDeviceId(int deviceId, std::function<void()> success,
                        std::function<void(const std::string &)> error) const;

 private:
  std::shared_ptr<ITelemetryRepository> repository_;
  std::shared_ptr<IDevicesService> devicesService_;
  std::shared_ptr<IUsersService> usersService_;
  std::shared_ptr<INotificationServiceClient> notificationClient_;
  double temperatureThreshold_;
};
