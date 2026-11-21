#pragma once

#include <functional>
#include <memory>
#include <string>

#include "devices/repositories/IDevicesRepository.h"
#include "devices/services/IDevicesService.h"

namespace drogon::orm { class DbClient; }

/// Сервис операций с устройствами; SQL выполняет репозиторий.
class DevicesService : public IDevicesService {
 public:
  /// Создаёт сервис с PostgreSQL-репозиторием.
  explicit DevicesService(std::shared_ptr<drogon::orm::DbClient> db);

  /// Создаёт сервис с переданным репозиторием.
  explicit DevicesService(std::shared_ptr<IDevicesRepository> repository);

  /// Регистрирует устройство и возвращает ID новой записи.
  void save(int ownerId, const std::string &name, const std::string &type,
            std::function<void(int)> success,
            std::function<void(const std::string &)> error) const;

  /// Удаляет устройство.
  void deleteById(int deviceId, std::function<void()> success,
                  std::function<void(const std::string &)> error) const;

  /// Возвращает владельца и имя устройства для уведомления.
  void getNotificationInfo(
      int deviceId, std::function<void(int, const std::string &)> success,
      std::function<void(const std::string &)> error) const override;

  /// Изменяет владельца и параметры устройства.
  void updateById(int deviceId, int ownerId, const std::string &name,
                  const std::string &type, std::function<void()> success,
                  std::function<void(const std::string &)> error) const;

  /// Возвращает данные устройства в JSON.
  void getById(int deviceId, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) const;

 private:
  std::shared_ptr<IDevicesRepository> repository_;
};
