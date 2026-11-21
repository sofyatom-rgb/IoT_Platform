#include <drogon/drogon.h>

#include "common/logging/DrogonLogging.h"

#include <filesystem>
#include <iostream>

#include "ingest/controllers/IngestController.h"

/// Запускает HTTP-адаптер датчиков.
int main(int, char *argv[]) {
  const auto configPath =
      std::filesystem::absolute(argv[0]).parent_path() / "AdapterConfig.json";
  drogon::app().loadConfigFile(configPath.string());
  iot::logging::configureDrogonLogging(configPath.parent_path() / "logs",
                                       "http_adapter");
  LOG_INFO << "[http_adapter] Starting HTTP Adapter";

  // Регистрируем маршруты
  IngestController ingestController;
  ingestController.registerRoutes();

  LOG_INFO << "[http_adapter] Forwarding requests to IoT Core at "
           << drogon::app().getCustomConfig()["iot_core_url"].asString();

  // Запускаем сервер
  LOG_INFO << "[http_adapter] HTTP server is ready";
  drogon::app().run();
  return 0;
}