#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>

#include "telemetry/repositories/ITelemetryRepository.h"

// Репозиторий: единственное место, где выполняется SQL.
class TelemetryRepository : public ITelemetryRepository {
 public:
  explicit TelemetryRepository(drogon::orm::DbClientPtr db);

  void save(int deviceId,
            double temperature,
            std::function<void()> success,
            std::function<void(const std::string &)> error) const override;

  void getByDeviceId(int deviceId,
                     std::function<void(const std::string &)> success,
                     std::function<void(const std::string &)> error) const override;

  void deleteByDeviceId(int deviceId,
                        std::function<void()> success,
                        std::function<void(const std::string &)> error) const override;

 private:
  drogon::orm::DbClientPtr db_;
};
