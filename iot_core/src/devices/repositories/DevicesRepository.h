#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>

#include "devices/repositories/IDevicesRepository.h"

// Репозиторий устройств: единственное место, где выполняется SQL для devices.
class DevicesRepository : public IDevicesRepository {
 public:
  explicit DevicesRepository(drogon::orm::DbClientPtr db);

  void save(int ownerId,
            const std::string &name,
            const std::string &type,
            std::function<void(int)> success,
            std::function<void(const std::string &)> error) const override;

  void deleteById(int deviceId,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const override;

  void getNotificationInfo(
      int deviceId,
      std::function<void(int, const std::string &)> success,
      std::function<void(const std::string &)> error) const override;

  void updateById(int deviceId,
                  int ownerId,
                  const std::string &name,
                  const std::string &type,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const override;

  void getById(int deviceId,
               std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) const override;

 private:
  drogon::orm::DbClientPtr db_;
};