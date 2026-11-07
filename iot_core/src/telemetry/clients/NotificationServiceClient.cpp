#include "telemetry/clients/NotificationServiceClient.h"

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {
iot::http::Request makeRequest(int deviceId, const std::string &chatId,
                               const std::string &email,
                               const std::string &deviceName,
                               double temperature, double threshold) {
  iot::http::Request request;
  request.method = iot::http::Method::Post;
  request.path = "/notifications/temperature";
  request.jsonBody = {{"device_id", static_cast<std::int64_t>(deviceId)},
                      {"chat_id", chatId},
                      {"email", email},
                      {"device_name", deviceName},
                      {"temperature", temperature},
                      {"threshold", threshold}};
  return request;
}

}  // namespace

NotificationServiceClient::NotificationServiceClient(
    std::string baseUrl,
    std::shared_ptr<iot::http::IHttpTransport> transport)
    : baseUrl_(std::move(baseUrl)), transport_(std::move(transport)) {
  if (!transport_) {
    throw std::invalid_argument(
        "NotificationServiceClient requires an HTTP transport");
  }
}

void NotificationServiceClient::sendTemperatureAlert(
    const std::string &chatId, const std::string &deviceName,
    double temperature, double threshold, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  sendTemperatureAlert(
      0, chatId, {}, deviceName, temperature, threshold,
      [success = std::move(success)](const NotificationChannelStatuses &) {
        if (success) success();
      },
      std::move(error));
}

void NotificationServiceClient::sendTemperatureAlert(
    int deviceId, const std::string &chatId, const std::string &email,
    const std::string &deviceName, double temperature, double threshold,
    std::function<void(const NotificationChannelStatuses &)> success,
    std::function<void(const std::string &)> error) const {
  if (baseUrl_.empty()) {
    if (error) error("Notification service URL is not configured");
    return;
  }
  const auto request =
      makeRequest(deviceId, chatId, email, deviceName, temperature, threshold);
  transport_->send(
      request,
      [chatId, email, success = std::move(success),
       error = std::move(error)](
          iot::http::Result result,
          std::optional<iot::http::Response> response) {
        if (result == iot::http::Result::Ok && response &&
            response->statusCode == 200) {
          NotificationChannelStatuses channels;
          if (const auto *telegram =
                  iot::http::stringField(response->jsonBody, "telegram")) {
            channels.telegram = *telegram;
          } else if (!chatId.empty()) {
            channels.telegram = "sent";
          }
          if (const auto *emailStatus =
                  iot::http::stringField(response->jsonBody, "email")) {
            channels.email = *emailStatus;
          } else if (!email.empty()) {
            channels.email = "sent";
          }
          if (success) success(channels);
          return;
        }

        const int status = response ? response->statusCode : 0;
        if (response) {
          if (const auto *message =
                  iot::http::stringField(response->jsonBody, "message");
              response->jsonParsed && message != nullptr &&
              !message->empty()) {
            if (error) {
              error("Notification service request failed (HTTP " +
                    std::to_string(status) + "): " + *message);
            }
            return;
          }
        }
        if (error) {
          error("Notification service request failed (HTTP " +
                std::to_string(status) + ")");
        }
      });
}

void NotificationServiceClient::clearTemperatureAlert(
    int deviceId, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  iot::http::Request request;
  request.method = iot::http::Method::Post;
  request.path = "/notifications/temperature/normal";
  request.jsonBody = {{"device_id", static_cast<std::int64_t>(deviceId)}};
  if (baseUrl_.empty()) {
    if (error) error("Notification service URL is not configured");
    return;
  }
  transport_->send(
      request,
      [success = std::move(success), error = std::move(error)](
          iot::http::Result result,
          std::optional<iot::http::Response> response) {
        if (result == iot::http::Result::Ok && response &&
            response->statusCode == 200) {
          if (success) success();
          return;
        }
        const int status = response ? response->statusCode : 0;
        if (error) {
          error("Notification service alert reset failed (HTTP " +
                std::to_string(status) + ")");
        }
      });
}
