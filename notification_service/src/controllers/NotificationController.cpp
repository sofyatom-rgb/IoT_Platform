#include "controllers/NotificationController.h"

#include <utility>

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
}

void NotificationController::sendTemperatureAlert(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json || !(*json)["chat_id"].isString() ||
      (*json)["chat_id"].asString().empty() ||
      !(*json)["device_name"].isString() ||
      !(*json)["temperature"].isNumeric() ||
      !(*json)["threshold"].isNumeric()) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Invalid notification payload";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  service_.sendTemperatureAlert(
      (*json)["chat_id"].asString(), (*json)["device_name"].asString(),
      (*json)["temperature"].asDouble(), (*json)["threshold"].asDouble(),
      [callback]() {
        Json::Value body;
        body["status"] = "ok";
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        callback(response);
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k502BadGateway);
        callback(response);
      });
}
