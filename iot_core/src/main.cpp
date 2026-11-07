#include <drogon/drogon.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "common/http/DrogonHttpTransport.h"
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

int main(int, char *argv[]) {
  const auto configDirectory =
      std::filesystem::absolute(argv[0]).parent_path().parent_path() / "config";
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

  // Composition root: здесь создаются реальные реализации зависимостей.
  const auto db = drogon::app().getDbClient();
  auto usersService = std::make_shared<UsersService>(
      std::make_shared<UsersRepository>(db));
  auto devicesService = std::make_shared<DevicesService>(
      std::make_shared<DevicesRepository>(db));
  auto notificationClient = std::make_shared<NotificationServiceClient>(
      customConfig["notification_service_url"].asString(),
      std::make_shared<iot::http::DrogonHttpTransport>(
          customConfig["notification_service_url"].asString()));
  auto telemetryService = std::make_shared<TelemetryService>(
      std::make_shared<TelemetryRepository>(db),
      customConfig["temperature_threshold"].asDouble(),
      devicesService, usersService, notificationClient);

  // Контроллеры получают готовые сервисы через конструкторы.
  TelemetryController telemetryController(telemetryService);
  telemetryController.registerRoutes();

  UsersController usersController(usersService);
  usersController.registerRoutes();

  DevicesController devicesController(devicesService);
  devicesController.registerRoutes();

  drogon::app().run();
  return 0;
}
