#include <drogon/drogon.h>

#include "controllers/telemetry/TelemetryController.h"

using namespace drogon;

int main() {
  // Параметры PostgreSQL из docker-compose.yml.
  orm::PostgresConfig dbConfig{"127.0.0.1", 5433, "iot_platform", "postgres",
                               "postgres", 4, "default", false, "", 3.0,
                               false, {}};
  app().addDbClient(dbConfig);

  // Контроллер регистрирует HTTP-маршруты. Клиент БД пока ещё не создан.
  TelemetryController controller;
  controller.registerRoutes();

  app().addListener("0.0.0.0", 8080).run();
  return 0;
}
