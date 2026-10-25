#include <drogon/drogon.h>

#include <filesystem>
#include <iostream>

#include "telemetry/controllers/TelemetryController.h"
#include "users/controllers/UsersController.h"
#include "devices/controllers/DevicesController.h"

int main(int, char *argv[]) {
  const auto configDirectory =
      std::filesystem::absolute(argv[0]).parent_path();
  drogon::app()
      .loadConfigFile((configDirectory / "PostgreSQLConfig.json").string())
      .loadConfigFile((configDirectory / "HTTPConfig.json").string());

  const auto &customConfig = drogon::app().getCustomConfig();
  if (!customConfig["temperature_threshold"].isNumeric() ||
      !customConfig["notification_service_url"].isString() ||
      customConfig["notification_service_url"].asString().empty()) {
    std::cerr << "HTTPConfig.json must define temperature_threshold and "
                 "notification_service_url under custom_config\n";
    return 1;
  }

  // Контроллеры регистрируют HTTP-маршруты.
  TelemetryController telemetryController;
  telemetryController.registerRoutes();

  UsersController usersController;
  usersController.registerRoutes();

  DevicesController devicesController;
  devicesController.registerRoutes();

  // Адрес и порт сервера берутся из config/HTTPConfig.json.
  drogon::app().run();
  return 0;
}
