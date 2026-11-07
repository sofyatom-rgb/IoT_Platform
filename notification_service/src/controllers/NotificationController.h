#pragma once

#include <drogon/drogon.h>

#include "services/NotificationService.h"

class NotificationController {
 public:
  explicit NotificationController(NotificationService service);

  void registerRoutes();

  // Kept public so the handler can be tested directly without starting a server.
  void sendTemperatureAlert(
      const drogon::HttpRequestPtr &request,
      std::function<void(const drogon::HttpResponsePtr &)> callback);

  NotificationService service_;
};
