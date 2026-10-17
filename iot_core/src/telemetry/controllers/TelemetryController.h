#pragma once

#include <drogon/drogon.h>

#include "telemetry/services/TelemetryService.h"

// HTTP-слой: принимает JSON и отправляет показание в сервис.
class TelemetryController {
 public:
  void registerRoutes();

  void create(
      const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);
};
