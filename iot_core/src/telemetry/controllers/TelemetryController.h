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
      const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

  void getTelemetry(
      const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

  void deleteTelemetry(
      const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

 private:
  std::shared_ptr<TelemetryService> service_;
};
