#include <drogon/drogon.h>
#include "controllers/RuleController.h"

/// Запускает сервис проверки правил.
int main() {
  drogon::app().loadConfigFile("config.json");

  RuleController controller;
  controller.registerRoutes();

  drogon::app().run();
  return 0;
}