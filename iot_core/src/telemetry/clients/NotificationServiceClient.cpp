#include "telemetry/clients/NotificationServiceClient.h"

#include <sstream>
#include <utility>

NotificationServiceClient::NotificationServiceClient(
    std::string baseUrl)
    : baseUrl_(std::move(baseUrl)) {}

void NotificationServiceClient::sendTemperatureAlert(
    const std::string &chatId,
    const std::string &email,
    const std::string &deviceName,
    double temperature,
    double threshold,
    std::function<void(const ChannelStatuses &)> success,
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
  body["email"] = email;
  body["device_name"] = deviceName;
  body["temperature"] = temperature;
  body["threshold"] = threshold;
  request->setBody(body.toStyledString());

  client->sendRequest(
      request,
      [success = std::move(success), error = std::move(error)](
          drogon::ReqResult result,
          const drogon::HttpResponsePtr &response) {
        const auto status =
            response ? static_cast<int>(response->getStatusCode()) : 0;
        if (response) {
          Json::CharReaderBuilder builder;
          Json::Value parsed;
          std::string parseErrors;
          const std::string responseBody(response->getBody());
          std::istringstream input(responseBody);
          if (Json::parseFromStream(builder, input, &parsed, &parseErrors)) {
            ChannelStatuses channels;
            if (parsed["telegram"].isString()) {
              channels.telegram = parsed["telegram"].asString();
            }
            if (parsed["email"].isString()) {
              channels.email = parsed["email"].asString();
            }
            if (result == drogon::ReqResult::Ok &&
                response->getStatusCode() == drogon::k200OK) {
              success(channels);
              return;
            }
            if (parsed["message"].isString() &&
                !parsed["message"].asString().empty()) {
              error("Notification service request failed (HTTP " +
                    std::to_string(status) + "): " +
                    parsed["message"].asString());
              return;
            }
          }
        }
        if (result == drogon::ReqResult::Ok && response &&
            response->getStatusCode() == drogon::k200OK) {
          success(ChannelStatuses{});
          return;
        }
        error("Notification service request failed (HTTP " +
              std::to_string(status) + ")");
      });
}
