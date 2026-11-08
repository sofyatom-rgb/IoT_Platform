#include "clients/NotificationClient.h"

#include <sstream>
#include <utility>

NotificationClient::NotificationClient(std::string baseUrl)
    : baseUrl_(std::move(baseUrl)) {}

void NotificationClient::sendTemperatureAlert(
    const std::string& chatId,
    const std::string& deviceName,
    double temperature,
    double threshold,
    std::function<void()> success,
    std::function<void(const std::string&)> error) const {
  auto client = drogon::HttpClient::newHttpClient(baseUrl_);
  auto req = drogon::HttpRequest::newHttpRequest();
  req->setMethod(drogon::Post);
  req->setPath("/notifications/temperature");
  req->setContentTypeCode(drogon::CT_APPLICATION_JSON);

  Json::Value body;
  body["chat_id"] = chatId;
  body["device_name"] = deviceName;
  body["temperature"] = temperature;
  body["threshold"] = threshold;
  req->setBody(body.toStyledString());

  client->sendRequest(
      req,
      [success = std::move(success), error = std::move(error)](
          drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
        if (result == drogon::ReqResult::Ok &&
            response->getStatusCode() == drogon::k200OK) {
          success();
        } else {
          int code = response ? static_cast<int>(response->getStatusCode()) : 0;
          error("Notification service returned HTTP " + std::to_string(code));
        }
      });
}