#pragma once

#include <functional>
#include <memory>
#include <string>

#include "telemetry/repositories/ITelemetryRepository.h"

namespace drogon::orm { class DbClient; }

/// Выполняет SQL-запросы к таблице телеметрии.
class TelemetryRepository : public ITelemetryRepository {
 public:
  /// Создаёт репозиторий с подключением к PostgreSQL.
  explicit TelemetryRepository(std::shared_ptr<drogon::orm::DbClient> db);

  /// Добавляет показание температуры.
  void save(int deviceId,
            double temperature,
            std::function<void()> success,
            std::function<void(const std::string &)> error) const override;

  /// Читает последние показания устройства.
  void getByDeviceId(int deviceId,
                     std::function<void(const std::string &)> success,
                     std::function<void(const std::string &)> error) const override;

  /// Удаляет все показания устройства.
  void deleteByDeviceId(int deviceId,
                        std::function<void()> success,
                        std::function<void(const std::string &)> error) const override;

 private:
  std::shared_ptr<drogon::orm::DbClient> db_;
};
