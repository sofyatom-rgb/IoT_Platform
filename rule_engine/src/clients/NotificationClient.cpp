#include "clients/NotificationClient.h"

#include <drogon/drogon.h>

#include <cstdint>
#include <stdexcept>
#include <utility>

NotificationClient::NotificationClient(
    std::string baseUrl,
    std::shared_ptr<iot::http::IHttpTransport> transport)
    : baseUrl_(std::move(baseUrl)), transport_(std::move(transport)) {
  if (!transport_) {
    throw std::invalid_argument("NotificationClient requires an HTTP transport");
  }
}

void NotificationClient::sendTemperatureAlert(
    int deviceId,
    const std::string& chatId,
    const std::string& email,
    const std::string& deviceName,
    double temperature,
    double threshold,
    std::function<void(const std::string&)> success,
    std::function<void(const std::string&)> error) const {
  if (baseUrl_.empty()) {
    LOG_ERROR << "[rule_engine][client] sendTemperatureAlert: notification service "
                 "URL is not configured";
    if (error) error("Notification service URL is not configured");
    return;
  }

  // Не логируем chat_id и email — это персональные данные.
  LOG_INFO << "[rule_engine][client] sendTemperatureAlert: started for device "
           << deviceId;

  iot::http::Request request;
  request.method = iot::http::Method::Post;
  request.path = "/notifications/temperature";
  request.jsonBody = {
      {"device_id", static_cast<std::int64_t>(deviceId)},
      {"chat_id", chatId},
      {"email", email},
      {"device_name", deviceName},
      {"temperature", temperature},
      {"threshold", threshold}};
  transport_->send(
      request,
      [deviceId, success = std::move(success), error = std::move(error)](
          iot::http::Result result,
          std::optional<iot::http::Response> response) {
        if (result == iot::http::Result::Ok && response &&
            response->statusCode == 200) {
          std::string status;
          if (const auto *telegram =
                  iot::http::stringField(response->jsonBody, "telegram")) {
            status = "telegram=" + *telegram;
          }
          if (const auto *emailStatus =
                  iot::http::stringField(response->jsonBody, "email")) {
            if (!status.empty()) status += "; ";
            status += "email=" + *emailStatus;
          }
          LOG_INFO << "[rule_engine][client] sendTemperatureAlert: completed for "
                      "device "
                   << deviceId << ", status="
                   << (status.empty() ? "sent" : status);
          if (success) success(status.empty() ? "sent" : status);
          return;
        }

        const int statusCode = response ? response->statusCode : 0;
        std::string message;
        if (response) {
          if (const auto *responseMessage =
                  iot::http::stringField(response->jsonBody, "message");
              response->jsonParsed && responseMessage != nullptr &&
              !responseMessage->empty()) {
            message = ": " + *responseMessage;
          }
        }
        LOG_ERROR << "[rule_engine][client] sendTemperatureAlert: failed for device "
                  << deviceId << ", status=" << statusCode;
        if (error) {
          error("Notification service returned HTTP " +
                std::to_string(statusCode) + message);
        }
      });
}

void NotificationClient::clearTemperatureAlert(
    int deviceId, std::function<void()> success,
    std::function<void(const std::string&)> error) const {
  if (baseUrl_.empty()) {
    LOG_ERROR << "[rule_engine][client] clearTemperatureAlert: notification service "
                 "URL is not configured";
    if (error) error("Notification service URL is not configured");
    return;
  }
  LOG_INFO << "[rule_engine][client] clearTemperatureAlert: started for device "
           << deviceId;
  iot::http::Request request;
  request.method = iot::http::Method::Post;
  request.path = "/notifications/temperature/normal";
  request.jsonBody = {{"device_id", static_cast<std::int64_t>(deviceId)}};
  transport_->send(
      request,
      [deviceId, success = std::move(success), error = std::move(error)](
          iot::http::Result result,
          std::optional<iot::http::Response> response) {
        if (result == iot::http::Result::Ok && response &&
            response->statusCode == 200) {
          LOG_INFO << "[rule_engine][client] clearTemperatureAlert: completed for "
                      "device "
                   << deviceId;
          if (success) success();
          return;
        }
        const int statusCode = response ? response->statusCode : 0;
        LOG_ERROR << "[rule_engine][client] clearTemperatureAlert: failed for device "
                  << deviceId << ", status=" << statusCode;
        if (error) {
          error("Notification service alert reset returned HTTP " +
                std::to_string(statusCode));
        }
      });
}