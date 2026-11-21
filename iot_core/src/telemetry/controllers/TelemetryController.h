#pragma once

#include <drogon/drogon.h>

#include <memory>

#include "telemetry/services/TelemetryService.h"

// HTTP-слой: принимает JSON и отправляет показание в сервис.
class TelemetryController {
 public:
  /// Создаёт контроллер с сервисом телеметрии.
  explicit TelemetryController(std::shared_ptr<TelemetryService> service);

  /// Регистрирует маршруты чтения и изменения телеметрии.
  void registerRoutes();

  /// Принимает новое показание температуры.
  void create(
      const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

  /// Возвращает историю показаний устройства.
  void getTelemetry(
      const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

  /// Удаляет историю показаний устройства.
  void deleteTelemetry(
      const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

 private:
  std::shared_ptr<TelemetryService> service_;
};
