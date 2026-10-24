#include "ingest/controllers/IngestController.h"

#include <string>
#include <utility>
#include <sstream>

void IngestController::registerRoutes() {
  // POST /ingest -> принять данные и отправить в iot_core.
  drogon::app().registerHandler(
      "/ingest",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        ingest(request, std::move(callback));
      },
      {drogon::Post});

  // POST /telemetry/query with JSON {"device_id": X} -> получить телеметрию.
  drogon::app().registerHandler(
      "/telemetry/query",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        getTelemetry(request, std::move(callback));
      },
      {drogon::Post});

  // DELETE /telemetry with JSON {"device_id": X} -> удалить телеметрию.
  drogon::app().registerHandler(
      "/telemetry",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        deleteTelemetry(request, std::move(callback));
      },
      {drogon::Delete});
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

void IngestController::getTelemetry(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json || !(*json)["device_id"].isInt() ||
      (*json)["device_id"].asInt() <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive integer device_id in JSON body";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  const int deviceId = (*json)["device_id"].asInt();

  // Получаем URL iot_core из конфигурации
  std::string iotCoreUrl = drogon::app().getCustomConfig()["iot_core_url"].asString();

  // Инициализируем сервис
  AdapterService service(iotCoreUrl);

  service.getTelemetry(
    deviceId,
    [callback](const std::string &responseBody) {
      Json::Value body;
      body["status"] = "ok";

      // Парсим строку JSON обратно в объект
      Json::CharReaderBuilder builder;
      std::string errs;
      Json::Value dataArray;
      std::istringstream iss(responseBody);
      if (Json::parseFromStream(builder, iss, &dataArray, &errs)) {
        body["data"] = dataArray;
      } else {
        // Если не распарсилось — кладём как строку (fallback)
        body["data_raw"] = responseBody;
      }

      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}

void IngestController::deleteTelemetry(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json || !(*json)["device_id"].isInt() ||
      (*json)["device_id"].asInt() <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive integer device_id in JSON body";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  const int deviceId = (*json)["device_id"].asInt();

  std::string iotCoreUrl = drogon::app().getCustomConfig()["iot_core_url"].asString();
  AdapterService service(iotCoreUrl);

  service.deleteTelemetry(
      deviceId,
      [callback]() {
        Json::Value body;
        body["status"] = "ok";
        body["message"] = "Telemetry deleted via adapter";
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}