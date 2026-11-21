#pragma once

#include <functional>
#include <memory>
#include <string>

#include "devices/repositories/IDevicesRepository.h"

namespace drogon::orm { class DbClient; }

/// Выполняет SQL-запросы к таблице устройств.
class DevicesRepository : public IDevicesRepository {
 public:
  /// Создаёт репозиторий с подключением к PostgreSQL.
  explicit DevicesRepository(std::shared_ptr<drogon::orm::DbClient> db);

  /// Добавляет устройство и возвращает его ID.
  void save(int ownerId,
            const std::string &name,
            const std::string &type,
            std::function<void(int)> success,
            std::function<void(const std::string &)> error) const override;

  /// Удаляет устройство по ID.
  void deleteById(int deviceId,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const override;

  /// Читает ID владельца и имя устройства.
  void getNotificationInfo(
      int deviceId,
      std::function<void(int, const std::string &)> success,
      std::function<void(const std::string &)> error) const override;

  /// Обновляет поля устройства.
  void updateById(int deviceId,
                  int ownerId,
                  const std::string &name,
                  const std::string &type,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const override;

  /// Читает данные устройства в JSON.
  void getById(int deviceId,
               std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) const override;

 private:
  std::shared_ptr<drogon::orm::DbClient> db_;
};
