#include "telemetry/controllers/TelemetryController.h"

#include <cmath>
#include <sstream>
#include <string>
#include <utility>

void TelemetryController::registerRoutes() {
  // POST /telemetry -> сохранить показание.
  drogon::app().registerHandler(
      "/telemetry",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        create(request, std::move(callback));
      },
      {drogon::Post});

  // GET /ping -> ответить pong.
  drogon::app().registerHandler(
      "/ping",
      [](const drogon::HttpRequestPtr &,
         std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setBody("pong\n");
        callback(response);
      },
      {drogon::Get});

  // POST /telemetry/query with JSON {"device_id": X} -> получить последние записи.
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

void TelemetryController::create(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();

  if (!json) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Invalid JSON";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  if (!(*json)["device_id"].isInt() || (*json)["device_id"].asInt() <= 0 ||
      !(*json)["temperature"].isNumeric() ||
      !std::isfinite((*json)["temperature"].asDouble())) {
    Json::Value body;
    body["status"] = "error";
    body["message"] =
        "Expected a positive integer device_id and finite numeric temperature";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  const auto config = drogon::app().getCustomConfig();
  NotificationServiceClient notificationClient(
      config["notification_service_url"].asString());
  TelemetryService service(
      drogon::app().getDbClient(),
      config["temperature_threshold"].asDouble(),
      std::move(notificationClient));
  service.save(
      (*json)["device_id"].asInt(), (*json)["temperature"].asDouble(),
      [callback](const std::string &notificationStatus) {
        Json::Value body;
        body["status"] = "ok";
        body["notification_status"] = notificationStatus;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}

void TelemetryController::getTelemetry(
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

  TelemetryService service(drogon::app().getDbClient());
  service.getByDeviceId(
      deviceId,
      [callback](const std::string &responseBody) {
        Json::CharReaderBuilder builder;
        std::string errs;
        Json::Value dataArray;
        std::istringstream iss(responseBody);
        if (Json::parseFromStream(builder, iss, &dataArray, &errs)) {
          auto resp = drogon::HttpResponse::newHttpJsonResponse(dataArray);
          callback(resp);
        } else {
          Json::Value body;
          body["status"] = "error";
          body["message"] = "Failed to parse telemetry data";
          callback(drogon::HttpResponse::newHttpJsonResponse(body));
        }
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}

void TelemetryController::deleteTelemetry(
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

  TelemetryService service(drogon::app().getDbClient());
  service.deleteByDeviceId(
      deviceId,
      [callback]() {
        Json::Value body;
        body["status"] = "ok";
        body["message"] = "Telemetry deleted";
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}
