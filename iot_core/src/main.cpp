#include <drogon/drogon.h>

#include "telemetry/controllers/TelemetryController.h"

int main() {
  // Сначала загружаем HTTP-настройки, затем подключение к PostgreSQL.
  drogon::app()
      .loadConfigFile("iot_core/src/telemetry/config/HTTPConfig.json")
      .loadConfigFile("iot_core/src/telemetry/config/PostgreSQLConfig.json");

  // Контроллер регистрирует HTTP-маршруты. Клиент БД пока ещё не создан.
  TelemetryController controller;
  controller.registerRoutes();

  // Адрес и порт сервера берутся из telemetry/config/HTTPConfig.json.
  drogon::app().run();
  return 0;
}
