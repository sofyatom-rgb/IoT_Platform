#include <drogon/drogon.h>

#include <filesystem>

#include "common/logging/DrogonLogging.h"
#include "ingest/controllers/IngestController.h"

/// Запускает HTTP-адаптер датчиков.
int main(int, char *argv[]) {
  const auto configDirectory =
      std::filesystem::absolute(argv[0]).parent_path().parent_path() / "config";
  drogon::app().loadConfigFile((configDirectory / "AdapterConfig.json").string());
  iot::logging::configureDrogonLogging(configDirectory / "logs", "http_adapter");
  LOG_INFO << "[http_adapter] Starting HTTP Adapter";

  // Регистрируем маршруты
  IngestController ingestController;
  ingestController.registerRoutes();

  LOG_INFO << "[http_adapter] Forwarding data to: "
           << drogon::app().getCustomConfig()["iot_core_url"].asString();
  LOG_INFO << "[http_adapter] HTTP server is ready";

  // Запускаем сервер
  drogon::app().run();
  return 0;
}