#include <drogon/drogon.h>

#include <filesystem>

#include "common/logging/DrogonLogging.h"
#include "controllers/RuleController.h"

int main(int, char *argv[]) {
  const auto configDirectory =
      std::filesystem::absolute(argv[0]).parent_path().parent_path() / "config";
  drogon::app().loadConfigFile((configDirectory / "RuleEngineConfig.json").string());
  iot::logging::configureDrogonLogging(configDirectory / "logs", "rule_engine");
  LOG_INFO << "[rule_engine] Starting Rule Engine";

  RuleController controller;
  controller.registerRoutes();

  LOG_INFO << "[rule_engine] HTTP server is ready";
  drogon::app().run();
  return 0;
}