#include <drogon/drogon.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "auth/controllers/AuthController.h"
#include "devices/controllers/DevicesController.h"
#include "telemetry/controllers/TelemetryController.h"
#include "users/controllers/UsersController.h"

int main(int, char *argv[]) {
  // Обязательные секреты берём из окружения, не из конфига и не из кода
  const char *pepper = std::getenv("PASSWORD_PEPPER");
  if (!pepper || std::string(pepper).empty()) {
    std::cerr << "PASSWORD_PEPPER is not set\n";
    return 1;
  }

  const char *jwtSecret = std::getenv("JWT_SECRET");
  if (!jwtSecret || std::string(jwtSecret).size() < 32) {
    std::cerr << "JWT_SECRET is not set or shorter than 32 characters\n";
    return 1;
  }

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

  TelemetryController telemetryController;
  telemetryController.registerRoutes();

  UsersController usersController;
  usersController.registerRoutes();

  DevicesController devicesController;
  devicesController.registerRoutes();

  AuthController authController;
  authController.registerRoutes();

  drogon::app().run();
  return 0;
}