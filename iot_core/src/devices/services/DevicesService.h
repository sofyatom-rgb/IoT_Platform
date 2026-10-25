#pragma once

#include <functional>
#include <string>

#include "devices/repositories/DevicesRepository.h"

// Сервисный слой устройств.
class DevicesService {
 public:
  explicit DevicesService(drogon::orm::DbClientPtr db);

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

 private:
  DevicesRepository repository_;
};