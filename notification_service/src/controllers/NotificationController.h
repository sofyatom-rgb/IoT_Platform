#pragma once

#include <drogon/drogon.h>

#include "services/NotificationService.h"

class NotificationController {
 public:
  explicit NotificationController(NotificationService service);

  void registerRoutes();

 private:
  void sendTemperatureAlert(
      const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);
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
