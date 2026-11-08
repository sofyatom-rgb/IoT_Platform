#include "telemetry/clients/RuleEngineClient.h"

#include <cstdint>
#include <stdexcept>
#include <utility>

RuleEngineClient::RuleEngineClient(
    std::string baseUrl,
    std::shared_ptr<iot::http::IHttpTransport> transport)
    : baseUrl_(std::move(baseUrl)), transport_(std::move(transport)) {
  if (!transport_) {
    throw std::invalid_argument("RuleEngineClient requires an HTTP transport");
  }
}

void RuleEngineClient::checkTelemetry(
    int deviceId, double temperature,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  if (baseUrl_.empty()) {
    if (error) error("Rule Engine URL is not configured");
    return;
  }

  iot::http::Request request;
  request.method = iot::http::Method::Post;
  request.path = "/telemetry";
  request.jsonBody = {{"device_id", static_cast<std::int64_t>(deviceId)},
                      {"temperature", temperature}};
  transport_->send(
      request,
      [success = std::move(success), error = std::move(error)](
          iot::http::Result result,
          std::optional<iot::http::Response> response) {
        if (result == iot::http::Result::Ok && response &&
            response->statusCode == 200) {
          const auto *status =
              iot::http::stringField(response->jsonBody, "notification_status");
          if (success) success(status == nullptr ? "checked" : *status);
          return;
        }

        const int statusCode = response ? response->statusCode : 0;
        if (response) {
          if (const auto *message =
                  iot::http::stringField(response->jsonBody, "message");
              response->jsonParsed && message != nullptr &&
              !message->empty()) {
            if (error) {
              error("Rule Engine request failed (HTTP " +
                    std::to_string(statusCode) + "): " + *message);
            }
            return;
          }
        }
        if (error) {
          error("Rule Engine request failed (HTTP " +
                std::to_string(statusCode) + ")");
        }
      });
}
