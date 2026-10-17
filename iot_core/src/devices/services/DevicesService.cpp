#include "devices/services/DevicesService.h"

#include <utility>

DevicesService::DevicesService(drogon::orm::DbClientPtr db)
    : repository_(std::move(db)) {}

void DevicesService::save(int ownerId,
                          const std::string &name,
                          const std::string &type,
                          std::function<void(int)> success,
                          std::function<void(const std::string &)> error) const {
  repository_.save(ownerId, name, type, std::move(success), std::move(error));
}