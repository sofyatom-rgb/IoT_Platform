#include "devices/services/DevicesService.h"

#include <stdexcept>
#include <utility>

DevicesService::DevicesService(std::shared_ptr<IDevicesRepository> repository)
    : repository_(std::move(repository)) {
  if (!repository_) throw std::invalid_argument("DevicesService requires a repository");
}

void DevicesService::save(int ownerId, const std::string &name,
                          const std::string &type,
                          std::function<void(int)> success,
                          std::function<void(const std::string &)> error) const {
  repository_->save(ownerId, name, type, std::move(success), std::move(error));
}

void DevicesService::deleteById(int deviceId, std::function<void()> success,
                                std::function<void(const std::string &)> error) const {
  repository_->deleteById(deviceId, std::move(success), std::move(error));
}

void DevicesService::getNotificationInfo(
    int deviceId, std::function<void(int, const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  repository_->getNotificationInfo(deviceId, std::move(success), std::move(error));
}

void DevicesService::updateById(
    int deviceId, int ownerId, const std::string &name, const std::string &type,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  repository_->updateById(deviceId, ownerId, name, type, std::move(success),
                           std::move(error));
}

void DevicesService::getById(int deviceId,
                             std::function<void(const std::string &)> success,
                             std::function<void(const std::string &)> error) const {
  repository_->getById(deviceId, std::move(success), std::move(error));
}
