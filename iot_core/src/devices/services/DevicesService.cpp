#include "devices/services/DevicesService.h"

#include <utility>

DevicesService::DevicesService(drogon::orm::DbClientPtr db)
    : repository_(std::move(db)) {}

namespace {
// Сервис живёт только до конца HTTP-хендлера, а запросы к БД асинхронные.
// Поэтому в callbacks нельзя захватывать this — захватываем копию
// репозитория (она содержит shared_ptr на DbClient).
using Repository = DevicesRepository;
}  // namespace

void DevicesService::save(int userId, int ownerId,
                          const std::string &name,
                          const std::string &type,
                          std::function<void(int)> success,
                          std::function<void(const std::string &)> error) const {
  Repository repository = repository_;
  repository.checkPermission(userId, "POST",
      [repository, ownerId, name, type, success, error](bool allowed) mutable {
        if (!allowed) {
          error("Permission denied: POST not allowed for your status");
          return;
        }
        repository.save(ownerId, name, type, std::move(success), std::move(error));
      },
      error);
}

void DevicesService::deleteById(int userId, int deviceId,
                                std::function<void()> success,
                                std::function<void(const std::string &)> error) const {
  Repository repository = repository_;
  repository.checkPermission(userId, "DELETE",
      [repository, deviceId, userId, success, error](bool allowed) mutable {
        if (!allowed) {
          error("Permission denied: DELETE not allowed for your status");
          return;
        }
        repository.checkDeviceOwnership(deviceId, userId,
            [repository, deviceId, success, error](bool owns) mutable {
              if (!owns) {
                error("Permission denied: you are not the owner of this device");
                return;
              }
              repository.deleteById(deviceId, std::move(success), std::move(error));
            },
            error);
      },
      error);
}

void DevicesService::getById(
    int userId,
    int deviceId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  Repository repository = repository_;
  repository.checkPermission(userId, "GET",
      [repository, deviceId, success, error](bool allowed) mutable {
        if (!allowed) {
          error("Permission denied: GET not allowed for your status");
          return;
        }
        repository.getById(deviceId, std::move(success), std::move(error));
      },
      error);
}

void DevicesService::listByOwner(
    int userId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  Repository repository = repository_;
  repository.getByOwnerId(userId, std::move(success), std::move(error));
}

void DevicesService::getNotificationInfo(
    int deviceId,
    std::function<void(int, const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  Repository repository = repository_;
  repository.getNotificationInfo(deviceId, std::move(success), std::move(error));
}

void DevicesService::updateById(int userId, int deviceId, int ownerId,
                                const std::string &name,
                                const std::string &type,
                                std::function<void()> success,
                                std::function<void(const std::string &)> error) const {
  Repository repository = repository_;
  repository.checkPermission(userId, "PUT",
      [repository, deviceId, userId, ownerId, name, type, success, error](bool allowed) mutable {
        if (!allowed) {
          error("Permission denied: PUT not allowed for your status");
          return;
        }
        repository.checkDeviceOwnership(deviceId, userId,
            [repository, deviceId, ownerId, name, type, success, error](bool owns) mutable {
              if (!owns) {
                error("Permission denied: you are not the owner of this device");
                return;
              }
              repository.updateById(deviceId, ownerId, name, type,
                                    std::move(success), std::move(error));
            },
            error);
      },
      error);
}
