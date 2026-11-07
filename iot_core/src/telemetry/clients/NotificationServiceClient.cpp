#include "telemetry/clients/NotificationServiceClient.h"

#include <stdexcept>
#include <utility>

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
  if (baseUrl_.empty()) {
    if (error) error("Notification service URL is not configured");
    return;
  }

  iot::http::Request request;
  request.method = iot::http::Method::Post;
  request.path = "/notifications/temperature";
  request.jsonBody = {{"chat_id", chatId},
                      {"device_name", deviceName},
                      {"temperature", temperature},
                      {"threshold", threshold}};

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
        if (response) {
          if (const auto *message =
                  iot::http::stringField(response->jsonBody, "message");
              response->jsonParsed && message != nullptr && !message->empty()) {
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
