#include "ingest/controllers/IngestController.h"

#include <utility>

void IngestController::registerRoutes() {
  // POST /ingest -> принять данные и отправить в iot_core.
  drogon::app().registerHandler(
      "/ingest",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        ingest(request, std::move(callback));
      },
      {drogon::Post});
}

void IngestController::ingest(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();

  if (!json) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Invalid JSON in Adapter";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  // Получаем URL iot_core из конфигурации
  std::string iotCoreUrl = drogon::app().getCustomConfig()["iot_core_url"].asString();
  
  // Инициализируем сервис
  AdapterService service(iotCoreUrl);
  
  service.processTelemetry(
      (*json)["device_id"].asInt(), 
      (*json)["temperature"].asDouble(),
      [callback]() {
        Json::Value body;
        body["status"] = "ok";
        body["message"] = "Data forwarded to IoT Core";
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}