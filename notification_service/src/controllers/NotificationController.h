#pragma once

#include <drogon/drogon.h>

#include "services/NotificationService.h"

/// Принимает HTTP-запросы и передаёт их сервису уведомлений.
class NotificationController {
 public:
  /// Создаёт контроллер с сервисом отправки уведомлений.
  explicit NotificationController(NotificationService service);

  /// Регистрирует маршрут отправки температурных уведомлений.
  void registerRoutes();

  // Публичные методы, чтобы обработчики можно было проверять без запуска сервера.
  void sendTemperatureAlert(
      const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);
  void clearTemperatureAlert(
      int deviceId,
      std::function<void(const drogon::HttpResponsePtr &)> callback);
  // Легаси-вариант: device_id передаётся в JSON-теле.
  void clearTemperatureAlert(
      const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);
  void sendEmailAlert(
      const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);
  void sendTestEmail(
      const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

  NotificationService service_;
};
