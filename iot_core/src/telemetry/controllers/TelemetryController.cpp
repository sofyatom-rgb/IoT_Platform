#include "telemetry/controllers/TelemetryController.h"
#include "common/validation/RequestValidation.h"

#include <memory>
#include <regex>
#include <stdexcept>

#include <sstream>
#include <string>
#include <utility>

namespace {
bool isUtcTimestamp(const Json::Value &value) {
  static const std::regex pattern(
      R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$)");
  return value.isString() && std::regex_match(value.asString(), pattern);
}
}  // namespace

TelemetryController::TelemetryController(std::shared_ptr<TelemetryService> service)
    : service_(std::move(service)) {
  if (!service_) {
    throw std::invalid_argument("TelemetryController requires a telemetry service");
  }
}

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

  // POST /telemetry/query supports latest, time-range, all-time, and cursor reads.
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
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  if (!(*json)["device_id"].isInt() ||
      !(*json)["temperature"].isNumeric() ||
      !iot::validation::isValidTelemetryInput(
          (*json)["device_id"].asInt(), (*json)["temperature"].asDouble())) {
    Json::Value body;
    body["status"] = "error";
    body["message"] =
        "Expected a positive integer device_id and finite numeric temperature";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  service_->save(
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
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
      });
}

void TelemetryController::getTelemetry(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json || !(*json)["device_id"].isInt() ||
      !iot::validation::isPositiveId((*json)["device_id"].asInt())) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive integer device_id in JSON body";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  const int deviceId = (*json)["device_id"].asInt();
  TelemetryQuery query;
  const bool hasFrom = json->isMember("from");
  const bool hasTo = json->isMember("to");
  const bool hasAfterId = json->isMember("after_id");
  const bool hasAllTime = json->isMember("all_time");
  if (hasFrom != hasTo ||
      (hasFrom &&
       (!isUtcTimestamp((*json)["from"]) || !isUtcTimestamp((*json)["to"]) ||
        (*json)["from"].asString() > (*json)["to"].asString())) ||
      (hasAfterId &&
       (!(*json)["after_id"].isInt() || (*json)["after_id"].asInt() < 0)) ||
      (hasAllTime && !(*json)["all_time"].isBool()) ||
      (hasAfterId && hasFrom) ||
      (hasAllTime && (hasFrom || hasAfterId))) {
    Json::Value body;
    body["status"] = "error";
    body["message"] =
        "Expected both valid UTC timestamps, all_time, or a non-negative after_id";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }
  if (hasFrom) {
    query.from = (*json)["from"].asString();
    query.to = (*json)["to"].asString();
  }
  if (hasAfterId) query.afterId = (*json)["after_id"].asInt();
  query.allTime = hasAllTime && (*json)["all_time"].asBool();

  service_->queryByDeviceId(
      deviceId, query,
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
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json || !(*json)["device_id"].isInt() ||
      !iot::validation::isPositiveId((*json)["device_id"].asInt())) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive integer device_id in JSON body";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  const int deviceId = (*json)["device_id"].asInt();

  service_->deleteByDeviceId(
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
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
      });
}
