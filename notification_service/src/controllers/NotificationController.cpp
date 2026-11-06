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
  const auto getOptionalString = [json](const char *key) -> std::string {
    if (!json || !(*json)[key].isString()) {
      return {};
    }
    return (*json)[key].asString();
  };
  const std::string chatId = getOptionalString("chat_id");
  const std::string email = getOptionalString("email");

  if (!json || !(*json)["device_name"].isString() ||
      !(*json)["temperature"].isNumeric() ||
      !(*json)["threshold"].isNumeric() || (chatId.empty() && email.empty())) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Invalid notification payload";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  service_.sendTemperatureAlert(
      chatId, email, (*json)["device_name"].asString(),
      (*json)["temperature"].asDouble(), (*json)["threshold"].asDouble(),
      [callback](const ChannelStatuses &statuses) {
        Json::Value body;
        body["status"] = "ok";
        body["telegram"] = statuses.telegram;
        body["email"] = statuses.email;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
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
