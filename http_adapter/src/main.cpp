#include <drogon/drogon.h>

#include <filesystem>
#include <iostream>

#include "ingest/controllers/IngestController.h"

int main(int, char *argv[]) {
  const auto configPath =
      std::filesystem::absolute(argv[0]).parent_path() / "AdapterConfig.json";
  drogon::app().loadConfigFile(configPath.string());

  // Регистрируем маршруты
  IngestController ingestController;
  ingestController.registerRoutes();

  std::cout << "HTTP Adapter starting on port 8081..." << std::endl;
  std::cout << "Forwarding data to: " 
            << drogon::app().getCustomConfig()["iot_core_url"].asString() 
            << std::endl;

  // Запускаем сервер
  drogon::app().run();
  return 0;
}