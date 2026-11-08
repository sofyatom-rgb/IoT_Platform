#include <drogon/drogon.h>
#include <filesystem>
#include "controllers/RuleController.h"

int main(int, char *argv[]) {
  const auto configPath =
      std::filesystem::absolute(argv[0]).parent_path().parent_path() /
      "config" / "RuleEngineConfig.json";
  drogon::app().loadConfigFile(configPath.string());

  RuleController controller;
  controller.registerRoutes();

  drogon::app().run();
  return 0;
}