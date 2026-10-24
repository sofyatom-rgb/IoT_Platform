#include <drogon/drogon.h>

#include "telemetry/controllers/TelemetryController.h"
#include "users/controllers/UsersController.h"
#include "devices/controllers/DevicesController.h"

int main() {
  // Сначала загружаем HTTP-настройки, затем подключение к PostgreSQL.
  drogon::app()
      .loadConfigFile("./bin/HTTPConfig.json")
      .loadConfigFile("./bin/PostgreSQLConfig.json");

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
