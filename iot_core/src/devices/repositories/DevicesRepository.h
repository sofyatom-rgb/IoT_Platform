#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>

// Репозиторий устройств: единственное место, где выполняется SQL для devices.
class DevicesRepository {
 public:
  explicit DevicesRepository(drogon::orm::DbClientPtr db);

  void save(int ownerId,
            const std::string &name,
            const std::string &type,
            std::function<void(int)> success,
            std::function<void(const std::string &)> error) const;

  void deleteById(int deviceId,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const;

  void getNotificationInfo(
      int deviceId,
      std::function<void(int, const std::string &)> success,
      std::function<void(const std::string &)> error) const;

  void updateById(int deviceId,
                  int ownerId,
                  const std::string &name,
                  const std::string &type,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const;

  void getById(int deviceId,
               std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) const;

 private:
  drogon::orm::DbClientPtr db_;
};