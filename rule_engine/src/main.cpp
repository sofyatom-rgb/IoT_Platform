#include <drogon/drogon.h>
#include <iostream>
#include <string>
#include <utility>

#include "controllers/RuleController.h"

int main() {
  // Загружаем конфигурацию HTTP и PostgreSQL
  drogon::app()
      .loadConfigFile("rule_engine/config/HTTPConfig.json")
      .loadConfigFile("rule_engine/config/PostgreSQLConfig.json");

  // Создаём контроллер правил и регистрируем маршруты
  RuleController controller;
  controller.registerRoutes();

  // Запускаем сервер
  drogon::app().run();
  return 0;
}