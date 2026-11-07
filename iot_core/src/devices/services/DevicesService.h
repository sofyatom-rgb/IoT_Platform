#pragma once

#include <functional>
#include <memory>
#include <string>

#include "devices/repositories/IDevicesRepository.h"
#include "devices/services/IDevicesService.h"

// Сервис устройств зависит от интерфейса репозитория, а не от PostgreSQL.
class DevicesService : public IDevicesService {
 public:
  explicit DevicesService(std::shared_ptr<IDevicesRepository> repository);

  void save(int ownerId, const std::string &name, const std::string &type,
            std::function<void(int)> success,
            std::function<void(const std::string &)> error) const override;
  void deleteById(int deviceId, std::function<void()> success,
                  std::function<void(const std::string &)> error) const override;
  void getNotificationInfo(int deviceId,
                           std::function<void(int, const std::string &)> success,
                           std::function<void(const std::string &)> error) const override;
  void updateById(int deviceId, int ownerId, const std::string &name,
                  const std::string &type, std::function<void()> success,
                  std::function<void(const std::string &)> error) const override;
  void getById(int deviceId, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) const override;

 private:
  std::shared_ptr<IDevicesRepository> repository_;
};
