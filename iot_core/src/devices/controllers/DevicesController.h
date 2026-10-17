#pragma once

#include <drogon/drogon.h>

#include "devices/services/DevicesService.h"

// HTTP-слой устройств: принимает JSON и отправляет в сервис.
class DevicesController {
 public:
  void registerRoutes();

  void create(const drogon::HttpRequestPtr &request,
              std::function<void(const drogon::HttpResponsePtr &)> callback);
};