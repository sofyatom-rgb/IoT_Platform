#include "devices/services/DevicesService.h"

#include "devices/repositories/DevicesRepository.h"

#include <utility>

DevicesService::DevicesService(std::shared_ptr<drogon::orm::DbClient> db)
    : DevicesService(std::make_shared<DevicesRepository>(std::move(db))) {}
