#include "controllers/NotificationController.h"

#include <cmath>
#include <string>
#include <utility>

namespace {
drogon::HttpResponsePtr jsonError(const std::string &message,
                                  drogon::HttpStatusCode status) {
  Json::Value body;
  body["status"] = "error";
  body["message"] = message;
  auto response = drogon::HttpResponse::newHttpJsonResponse(body);
  response->setStatusCode(status);
  return response;
}

std::string optionalString(const Json::Value &body, const char *key) {
  return body[key].isString() ? body[key].asString() : std::string{};
}

void sendResult(
    std::function<void(const drogon::HttpResponsePtr &)> callback,
    const std::string &channel) {
  Json::Value body;
  body["status"] = "ok";
  body[channel] = "sent";
  callback(drogon::HttpResponse::newHttpJsonResponse(body));
}
}  // namespace

NotificationController::NotificationController(NotificationService service)
    : service_(std::move(service)) {}

void NotificationController::registerRoutes() {
  drogon::app().registerHandler(
      "/notifications/temperature",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        sendTemperatureAlert(request, std::move(callback));
      },
      {drogon::Post});
  drogon::app().registerHandler(
      "/notifications/temperature/normal",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        clearTemperatureAlert(request, std::move(callback));
      },
      {drogon::Post});
  drogon::app().registerHandler(
      "/notifications/email",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        sendEmailAlert(request, std::move(callback));
      },
      {drogon::Post});
  drogon::app().registerHandler(
      "/notifications/email/test",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        sendTestEmail(request, std::move(callback));
      },
      {drogon::Post});
}

void NotificationController::sendTemperatureAlert(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json || !(*json)["device_name"].isString() ||
      !(*json)["temperature"].isNumeric() ||
      !(*json)["threshold"].isNumeric() ||
      !std::isfinite((*json)["temperature"].asDouble()) ||
      !std::isfinite((*json)["threshold"].asDouble()) ||
      (!(*json)["device_id"].isNull() &&
       (!(*json)["device_id"].isInt() || (*json)["device_id"].asInt() <= 0))) {
    callback(jsonError("Invalid temperature notification payload",
                       drogon::k400BadRequest));
    return;
  }

  const int deviceId =
      (*json)["device_id"].isInt() ? (*json)["device_id"].asInt() : 0;
  const std::string chatId = optionalString(*json, "chat_id");
  const std::string email = optionalString(*json, "email");
  if (chatId.empty() && email.empty()) {
    callback(jsonError("At least one recipient channel is required",
                       drogon::k400BadRequest));
    return;
  }

  service_.sendTemperatureAlert(
      deviceId, chatId, email, (*json)["device_name"].asString(),
      (*json)["temperature"].asDouble(), (*json)["threshold"].asDouble(),
      [callback](const ChannelStatuses &statuses) {
        Json::Value body;
        body["status"] = "ok";
        body["telegram"] = statuses.telegram;
        body["email"] = statuses.email;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      [callback](const std::string &error) {
        callback(jsonError(error, drogon::k502BadGateway));
      });
}

void NotificationController::clearTemperatureAlert(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json || !(*json)["device_id"].isInt() ||
      (*json)["device_id"].asInt() <= 0) {
    callback(jsonError("Expected a positive device_id",
                       drogon::k400BadRequest));
    return;
  }
  service_.clearTemperatureAlert((*json)["device_id"].asInt());
  Json::Value body;
  body["status"] = "ok";
  body["email"] = "alert_cleared";
  callback(drogon::HttpResponse::newHttpJsonResponse(body));
}

void NotificationController::sendEmailAlert(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json || !(*json)["email"].isString() ||
      !(*json)["event_type"].isString() ||
      !(*json)["sensor_name"].isString() ||
      !(*json)["reading_value"].isString()) {
    callback(jsonError(
        "Expected email, event_type, sensor_name, and reading_value strings",
        drogon::k400BadRequest));
    return;
  }

  EmailAlertData alert;
  alert.recipientName = optionalString(*json, "recipient_name");
  alert.platformName = optionalString(*json, "platform_name");
  alert.eventType = optionalString(*json, "event_type");
  alert.sensorName = optionalString(*json, "sensor_name");
  alert.sensorId = optionalString(*json, "sensor_id");
  alert.readingValue = optionalString(*json, "reading_value");
  alert.unit = optionalString(*json, "unit");
  alert.threshold = optionalString(*json, "threshold");
  alert.location = optionalString(*json, "location");
  alert.occurredAt = optionalString(*json, "occurred_at");
  alert.severity = optionalString(*json, "severity");
  alert.description = optionalString(*json, "description");
  if (alert.platformName.empty()) {
    alert.platformName = "IoT Monitor";
  }
  if (alert.severity.empty()) {
    alert.severity = "INFO";
  }
  if (alert.severity != "INFO" && alert.severity != "WARNING" &&
      alert.severity != "CRITICAL") {
    callback(jsonError("severity must be INFO, WARNING, or CRITICAL",
                       drogon::k400BadRequest));
    return;
  }

  service_.sendEmailAlert(
      (*json)["email"].asString(), alert,
      [callback]() { sendResult(std::move(callback), "email"); },
      [callback](const std::string &error) {
        callback(jsonError(error, drogon::k502BadGateway));
      });
}

void NotificationController::sendTestEmail(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  const std::string email =
      json ? optionalString(*json, "email") : request->getParameter("email");
  if (email.empty()) {
    callback(jsonError("Expected a non-empty email string",
                       drogon::k400BadRequest));
    return;
  }
  service_.sendTestEmail(
      email,
      [callback]() { sendResult(std::move(callback), "email"); },
      [callback](const std::string &error) {
        callback(jsonError(error, drogon::k502BadGateway));
      });
}
