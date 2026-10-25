#include "telemetry/clients/NotificationServiceClient.h"

#include <sstream>
#include <utility>

NotificationServiceClient::NotificationServiceClient(
    std::string baseUrl)
    : baseUrl_(std::move(baseUrl)) {}

void NotificationServiceClient::sendTemperatureAlert(
    const std::string &chatId,
    const std::string &deviceName,
    double temperature,
    double threshold,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  if (baseUrl_.empty()) {
    error("Notification service URL is not configured");
    return;
  }
  auto client = drogon::HttpClient::newHttpClient(baseUrl_);
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setMethod(drogon::Post);
  request->setPath("/notifications/temperature");
  request->setContentTypeCode(drogon::CT_APPLICATION_JSON);

  Json::Value body;
  body["chat_id"] = chatId;
  body["device_name"] = deviceName;
  body["temperature"] = temperature;
  body["threshold"] = threshold;
  request->setBody(body.toStyledString());

  client->sendRequest(
      request,
      [success = std::move(success), error = std::move(error)](
          drogon::ReqResult result,
          const drogon::HttpResponsePtr &response) {
        if (result == drogon::ReqResult::Ok && response &&
            response->getStatusCode() == drogon::k200OK) {
          success();
          return;
        }

        const auto status =
            response ? static_cast<int>(response->getStatusCode()) : 0;
        if (response) {
          Json::CharReaderBuilder builder;
          Json::Value body;
          std::string parseErrors;
          const std::string responseBody(response->getBody());
          std::istringstream input(responseBody);
          if (Json::parseFromStream(builder, input, &body, &parseErrors) &&
              body["message"].isString() &&
              !body["message"].asString().empty()) {
            error("Notification service request failed (HTTP " +
                  std::to_string(status) + "): " +
                  body["message"].asString());
            return;
          }
        }
        error("Notification service request failed (HTTP " +
              std::to_string(status) + ")");
      });
}
