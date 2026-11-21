#include <drogon/drogon.h>

#include "common/logging/DrogonLogging.h"

#include <filesystem>
#include "controllers/RuleController.h"

/// Запускает сервис проверки правил.
int main(int, char *argv[]) {
  const auto configPath =
      std::filesystem::absolute(argv[0]).parent_path() / "config.json";
  drogon::app().loadConfigFile(configPath.string());
  iot::logging::configureDrogonLogging(configPath.parent_path() / "logs",
                                       "rule_engine");
  LOG_INFO << "[rule_engine] Starting Rule Engine";

  const auto database = drogon::app().getDbClient();
  (void)database;
  LOG_INFO << "[rule_engine] PostgreSQL client configured";

  RuleController controller;
  controller.registerRoutes();

  LOG_INFO << "[rule_engine] HTTP server is ready";
  drogon::app().run();
  return 0;
}