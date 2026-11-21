#include "devices/services/DevicesService.h"

#include <utility>

DevicesService::DevicesService(drogon::orm::DbClientPtr db)
    : repository_(std::move(db)) {}

void DevicesService::save(int userId, int ownerId,
                          const std::string &name,
                          const std::string &type,
                          std::function<void(int)> success,
                          std::function<void(const std::string &)> error) const {
  repository_.checkPermission(userId, "POST",
      [this, ownerId, name, type, success, error](bool allowed) {
        if (!allowed) {
          error("Permission denied: POST not allowed for your status");
          return;
        }
        repository_.save(ownerId, name, type, std::move(success), std::move(error));
      },
      error);
}

void DevicesService::deleteById(int userId, int deviceId,
                                std::function<void()> success,
                                std::function<void(const std::string &)> error) const {
  repository_.checkPermission(userId, "DELETE",
      [this, deviceId, userId, success, error](bool allowed) {
        if (!allowed) {
          error("Permission denied: DELETE not allowed for your status");
          return;
        }
        repository_.checkDeviceOwnership(deviceId, userId,
            [this, deviceId, success, error](bool owns) {
              if (!owns) {
                error("Permission denied: you are not the owner of this device");
                return;
              }
              repository_.deleteById(deviceId, std::move(success), std::move(error));
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
  repository_.checkPermission(userId, "GET",
      [this, deviceId, success, error](bool allowed) {
        if (!allowed) {
          error("Permission denied: GET not allowed for your status");
          return;
        }
        repository_.getById(deviceId, std::move(success), std::move(error));
      },
      error);
}

void DevicesService::listByOwner(
    int userId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  repository_.getByOwnerId(userId, std::move(success), std::move(error));
}

void DevicesService::getNotificationInfo(
    int deviceId,
    std::function<void(int, const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  repository_.getNotificationInfo(deviceId, std::move(success),
                                  std::move(error));
}

void DevicesService::updateById(int userId, int deviceId, int ownerId,
                                const std::string &name,
                                const std::string &type,
                                std::function<void()> success,
                                std::function<void(const std::string &)> error) const {
  repository_.checkPermission(userId, "PUT",
      [this, deviceId, userId, ownerId, name, type, success, error](bool allowed) {
        if (!allowed) {
          error("Permission denied: PUT not allowed for your status");
          return;
        }
        repository_.checkDeviceOwnership(deviceId, userId,
            [this, deviceId, ownerId, name, type, success, error](bool owns) {
              if (!owns) {
                error("Permission denied: you are not the owner of this device");
                return;
              }
              repository_.updateById(deviceId, ownerId, name, type,
                                     std::move(success), std::move(error));
            },
            error);
      },
      error);
}