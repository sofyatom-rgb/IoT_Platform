#include <drogon/drogon.h>
#include "controllers/RuleController.h"

int main() {
  drogon::app().loadConfigFile("config.json");

  RuleController controller;
  controller.registerRoutes();

  drogon::app().run();
  return 0;
}