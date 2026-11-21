#pragma once

#include <drogon/drogon.h>

#include <memory>

#include "telemetry/services/TelemetryService.h"

// HTTP-слой: принимает JSON и отправляет показание в сервис.
class TelemetryController {
 public:
  explicit TelemetryController(std::shared_ptr<TelemetryService> service);

  void registerRoutes();

  void create(
      int deviceId, const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

  void getTelemetry(
      int deviceId,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

  void deleteTelemetry(
      int deviceId,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

 private:
  std::shared_ptr<TelemetryService> service_;
};
