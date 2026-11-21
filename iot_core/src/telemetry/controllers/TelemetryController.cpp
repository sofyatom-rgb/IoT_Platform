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
  // id устройства в URI.
  drogon::app().registerHandler(
      "/devices/{id}/telemetry",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int deviceId) {
        // setParameter отключает разбор query-строки, поэтому добавляем id
        // именно как query-параметр — фильтры истории должны читаться.
        request->setQueryParameter("device_id", std::to_string(deviceId));
        create(request, std::move(callback));
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/devices/{id}/telemetry",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int deviceId) {
        // setParameter отключает разбор query-строки, поэтому добавляем id
        // именно как query-параметр — фильтры истории должны читаться.
        request->setQueryParameter("device_id", std::to_string(deviceId));
        getTelemetry(request, std::move(callback));
      },
      {drogon::Get});

  drogon::app().registerHandler(
      "/devices/{id}/telemetry",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int deviceId) {
        // setParameter отключает разбор query-строки, поэтому добавляем id
        // именно как query-параметр — фильтры истории должны читаться.
        request->setQueryParameter("device_id", std::to_string(deviceId));
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

  // device_id передаётся в URI, в теле только temperature.
  int deviceId = 0;
  const std::string idParam = request->getParameter("device_id");
  try {
    deviceId = std::stoi(idParam);
  } catch (...) {
    deviceId = 0;
  }

  if (!iot::validation::isValidTelemetryInput(
          deviceId, (*json)["temperature"].isNumeric()
                        ? (*json)["temperature"].asDouble()
                        : 0.0) ||
      !(*json)["temperature"].isNumeric()) {
    Json::Value body;
    body["status"] = "error";
    body["message"] =
        "Expected a positive device ID in the URI and finite numeric temperature";
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
  int deviceId = 0;
  const std::string idParam = request->getParameter("device_id");
  if (!idParam.empty()) {
    try {
      deviceId = std::stoi(idParam);
    } catch (...) {
      deviceId = 0;
    }
  }
  if (!iot::validation::isPositiveId(deviceId)) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive integer device_id in the URI";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  // Фильтры истории передаются query-параметрами.
  const std::string from = request->getParameter("from");
  const std::string to = request->getParameter("to");
  const std::string afterIdParam = request->getParameter("after_id");
  const std::string allTimeParam = request->getParameter("all_time");
  const bool hasFrom = !from.empty();
  const bool hasTo = !to.empty();
  const bool hasAfterId = !afterIdParam.empty();
  const bool hasAllTime = !allTimeParam.empty();

  int afterId = 0;
  bool badAfterId = false;
  if (hasAfterId) {
    try {
      afterId = std::stoi(afterIdParam);
      badAfterId = afterId < 0;
    } catch (...) {
      badAfterId = true;
    }
  }
  const bool badAllTime =
      hasAllTime && allTimeParam != "true" && allTimeParam != "false" &&
      allTimeParam != "1" && allTimeParam != "0";

  if (hasFrom != hasTo ||
      (hasFrom && (!isUtcTimestamp(Json::Value(from)) ||
                   !isUtcTimestamp(Json::Value(to)) || from > to)) ||
      badAfterId || badAllTime || (hasAfterId && hasFrom) ||
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

  TelemetryQuery query;
  if (hasFrom) {
    query.from = from;
    query.to = to;
  }
  if (hasAfterId) query.afterId = afterId;
  query.allTime =
      hasAllTime && (allTimeParam == "true" || allTimeParam == "1");

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
  int deviceId = 0;
  const std::string idParam = request->getParameter("device_id");
  try {
    deviceId = std::stoi(idParam);
  } catch (...) {
    deviceId = 0;
  }
  if (!iot::validation::isPositiveId(deviceId)) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive device ID in the URI";
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
