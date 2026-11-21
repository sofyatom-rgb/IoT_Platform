#include <drogon/drogon.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "devices/controllers/DevicesController.h"
#include "devices/repositories/DevicesRepository.h"
#include "devices/services/DevicesService.h"
#include "telemetry/clients/NotificationServiceClient.h"
#include "telemetry/controllers/TelemetryController.h"
#include "telemetry/repositories/TelemetryRepository.h"
#include "telemetry/services/TelemetryService.h"
#include "users/controllers/UsersController.h"
#include "users/repositories/UsersRepository.h"
#include "users/services/UsersService.h"

/// Загружает конфигурацию, собирает зависимости IoT Core и запускает HTTP-сервер.
int main(int, char *argv[]) {
  const char *pepper = std::getenv("PASSWORD_PEPPER");
  if (!pepper || std::string(pepper).empty()) {
    LOG_FATAL << "PASSWORD_PEPPER is not set";
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

  // Создаём реальные зависимости приложения в одной точке композиции.
  const auto database = drogon::app().getDbClient();
  auto usersService = std::make_shared<UsersService>(
      std::make_shared<UsersRepository>(database));
  auto devicesService = std::make_shared<DevicesService>(
      std::make_shared<DevicesRepository>(database));
  auto notificationClient = std::make_shared<NotificationServiceClient>(
      customConfig["notification_service_url"].asString());
  auto telemetryService = std::make_shared<TelemetryService>(
      std::make_shared<TelemetryRepository>(database), devicesService,
      usersService, notificationClient,
      customConfig["temperature_threshold"].asDouble());

  // Контроллеры получают сервисы через конструкторы и регистрируют маршруты.
  TelemetryController telemetryController(telemetryService);
  telemetryController.registerRoutes();

  UsersController usersController(usersService);
  usersController.registerRoutes();

  DevicesController devicesController(devicesService);
  devicesController.registerRoutes();

  drogon::app().run();
  return 0;
}
