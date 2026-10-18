#include <drogon/drogon.h>
#include <iostream>

#include "ingest/controllers/IngestController.h"

int main() {
  // Загружаем настройки адаптера
  drogon::app().loadConfigFile("./bin/AdapterConfig.json");

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