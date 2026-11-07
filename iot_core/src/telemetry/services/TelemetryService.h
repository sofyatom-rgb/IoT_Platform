#pragma once

#include <functional>
#include <memory>
#include <string>

#include "devices/services/IDevicesService.h"
#include "telemetry/clients/INotificationServiceClient.h"
#include "telemetry/repositories/ITelemetryRepository.h"
#include "users/services/IUsersService.h"

// Сценарий сохранения телеметрии и отправки уведомления.
// Все внешние зависимости внедряются через порты: тесты могут подменить БД,
// поиск владельца/пользователя и HTTP-клиент обычными fake-объектами.
class TelemetryService {
 public:
  // Для сценариев чтения/удаления, которым нужен только репозиторий.
  explicit TelemetryService(std::shared_ptr<ITelemetryRepository> repository);

  // Полная конфигурация для сохранения телеметрии и возможного уведомления.
  TelemetryService(std::shared_ptr<ITelemetryRepository> repository,
                   double temperatureThreshold,
                   std::shared_ptr<IDevicesService> devices,
                   std::shared_ptr<IUsersService> users,
                   std::shared_ptr<INotificationServiceClient> notificationClient);

  void save(int deviceId, double temperature,
            std::function<void(const std::string &)> success,
            std::function<void(const std::string &)> error) const;
  void getByDeviceId(int deviceId,
                     std::function<void(const std::string &)> success,
                     std::function<void(const std::string &)> error) const;
  void deleteByDeviceId(int deviceId, std::function<void()> success,
                        std::function<void(const std::string &)> error) const;

 private:
  std::shared_ptr<ITelemetryRepository> repository_;
  double temperatureThreshold_{0.0};
  std::shared_ptr<IDevicesService> devices_;
  std::shared_ptr<IUsersService> users_;
  std::shared_ptr<INotificationServiceClient> notificationClient_;
};
