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

  // REST-варианты: id устройства в URI.
  drogon::app().registerHandler(
      "/devices/{id}/telemetry",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int deviceId) {
        request->setParameter("device_id", std::to_string(deviceId));
        create(request, std::move(callback));
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/devices/{id}/telemetry",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int deviceId) {
        request->setParameter("device_id", std::to_string(deviceId));
        getTelemetry(request, std::move(callback));
      },
      {drogon::Get});

  drogon::app().registerHandler(
      "/devices/{id}/telemetry",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int deviceId) {
        request->setParameter("device_id", std::to_string(deviceId));
        deleteTelemetry(request, std::move(callback));
      },
      {drogon::Delete});

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

  // REST передаёт id в URI, старый вариант — в JSON.
  int deviceId = 0;
  const std::string idParam = request->getParameter("device_id");
  if (!idParam.empty()) {
    try {
      deviceId = std::stoi(idParam);
    } catch (...) {
      deviceId = 0;
    }
  } else if (json && (*json)["device_id"].isInt()) {
    deviceId = (*json)["device_id"].asInt();
  }

  if (!iot::validation::isValidTelemetryInput(
          deviceId, (*json)["temperature"].isNumeric()
                        ? (*json)["temperature"].asDouble()
                        : 0.0) ||
      !(*json)["temperature"].isNumeric()) {
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
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  int deviceId = 0;
  const std::string idParam = request->getParameter("device_id");
  if (!idParam.empty()) {
    try {
      deviceId = std::stoi(idParam);
    } catch (...) {
      deviceId = 0;
    }
  } else if (json && (*json)["device_id"].isInt()) {
    deviceId = (*json)["device_id"].asInt();
  }
  if (!iot::validation::isPositiveId(deviceId)) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive integer device_id";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  // REST-запрос приходит без тела, поэтому используем обычный запрос истории.
  if (!json) {
    service_->queryByDeviceId(
        deviceId, TelemetryQuery{},
        [callback](const std::string &responseBody) {
          Json::CharReaderBuilder builder;
          std::string errs;
          Json::Value dataArray;
          std::istringstream iss(responseBody);
          if (Json::parseFromStream(builder, iss, &dataArray, &errs)) {
            callback(drogon::HttpResponse::newHttpJsonResponse(dataArray));
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
    return;
  }
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
  int deviceId = 0;
  const std::string idParam = request->getParameter("device_id");
  if (!idParam.empty()) {
    try {
      deviceId = std::stoi(idParam);
    } catch (...) {
      deviceId = 0;
    }
  } else if (json && (*json)["device_id"].isInt()) {
    deviceId = (*json)["device_id"].asInt();
  }
  if (!iot::validation::isPositiveId(deviceId)) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive integer device_id";
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
