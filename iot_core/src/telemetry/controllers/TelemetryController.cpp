#include "telemetry/controllers/TelemetryController.h"
#include "common/validation/RequestValidation.h"

#include <memory>
#include <stdexcept>

#include <sstream>
#include <string>
#include <utility>

TelemetryController::TelemetryController(std::shared_ptr<TelemetryService> service)
    : service_(std::move(service)) {
  if (!service_) {
    throw std::invalid_argument("TelemetryController requires a telemetry service");
  }
}

void TelemetryController::registerRoutes() {
  // POST /devices/{id}/telemetry -> добавить показание устройства.
  drogon::app().registerHandler(
      "/devices/{id}/telemetry",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int deviceId) {
        create(deviceId, request, std::move(callback));
      },
      {drogon::Post});

  // GET /health -> проверить доступность сервиса.
  drogon::app().registerHandler(
      "/health",
      [](const drogon::HttpRequestPtr &,
         std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        Json::Value body;
        body["status"] = "ok";
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      {drogon::Get});

  // GET /devices/{id}/telemetry -> получить показания устройства.
  drogon::app().registerHandler(
      "/devices/{id}/telemetry",
      [this](const drogon::HttpRequestPtr &,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int deviceId) {
        getTelemetry(deviceId, std::move(callback));
      },
      {drogon::Get});

  // DELETE /devices/{id}/telemetry -> удалить показания устройства.
  drogon::app().registerHandler(
      "/devices/{id}/telemetry",
      [this](const drogon::HttpRequestPtr &,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int deviceId) {
        deleteTelemetry(deviceId, std::move(callback));
      },
      {drogon::Delete});
}

void TelemetryController::create(
    int deviceId, const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  if (!iot::validation::isPositiveId(deviceId)) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive device ID in the URL";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  const auto json = request->getJsonObject();

  if (!json) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Invalid JSON";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  if (!(*json)["temperature"].isNumeric() ||
      !iot::validation::isValidTelemetryInput(
          deviceId, (*json)["temperature"].asDouble())) {
    Json::Value body;
    body["status"] = "error";
    body["message"] =
        "Expected a finite numeric temperature";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  service_->save(
      deviceId, (*json)["temperature"].asDouble(),
      [callback](const std::string &notificationStatus) {
        Json::Value body;
        body["status"] = "ok";
        body["notification_status"] = notificationStatus;
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k201Created);
        callback(response);
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
      });
}

void TelemetryController::getTelemetry(
    int deviceId,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  if (!iot::validation::isPositiveId(deviceId)) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive device ID in the URL";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  service_->getByDeviceId(
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
          auto response = drogon::HttpResponse::newHttpJsonResponse(body);
          response->setStatusCode(drogon::k500InternalServerError);
          callback(response);
        }
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
      });
}

void TelemetryController::deleteTelemetry(
    int deviceId,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  if (!iot::validation::isPositiveId(deviceId)) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive device ID in the URL";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  service_->deleteByDeviceId(
      deviceId,
      [callback]() {
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(drogon::k204NoContent);
        callback(response);
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
      });
}
