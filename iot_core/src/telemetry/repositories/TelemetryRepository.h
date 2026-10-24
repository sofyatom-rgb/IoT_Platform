#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>

// Репозиторий: единственное место, где выполняется SQL.
class TelemetryRepository {
 public:
  explicit TelemetryRepository(drogon::orm::DbClientPtr db);

  void save(int deviceId,
            double temperature,
            std::function<void()> success,
            std::function<void(const std::string &)> error) const;

  void getByDeviceId(int deviceId,
                     std::function<void(const std::string &)> success,
                     std::function<void(const std::string &)> error) const;

 private:
  drogon::orm::DbClientPtr db_;
};
