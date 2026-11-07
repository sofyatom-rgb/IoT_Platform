#pragma once

#include <functional>
#include <string>

// Port репозитория устройств; изолирует сервис от PostgreSQL.
class IDevicesRepository {
 public:
  virtual ~IDevicesRepository() = default;
  virtual void save(int ownerId, const std::string &name, const std::string &type,
                    std::function<void(int)> success,
                    std::function<void(const std::string &)> error) const = 0;
  virtual void deleteById(int deviceId, std::function<void()> success,
                          std::function<void(const std::string &)> error) const = 0;
  virtual void getNotificationInfo(
      int deviceId, std::function<void(int, const std::string &)> success,
      std::function<void(const std::string &)> error) const = 0;
  virtual void updateById(int deviceId, int ownerId, const std::string &name,
                          const std::string &type, std::function<void()> success,
                          std::function<void(const std::string &)> error) const = 0;
  virtual void getById(int deviceId,
                       std::function<void(const std::string &)> success,
                       std::function<void(const std::string &)> error) const = 0;
};
