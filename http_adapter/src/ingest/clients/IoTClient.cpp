#include "ingest/clients/IoTClient.h"

#include <stdexcept>
#include <utility>

IoTClient::IoTClient(std::shared_ptr<iot::http::IHttpTransport> transport)
    : transport_(std::move(transport)) {
  if (!transport_) {
    throw std::invalid_argument("IoTClient requires an HTTP transport");
  }
}

void IoTClient::sendTelemetry(
    int deviceId, double temperature,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
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
          if (status != nullptr) {
            if (success) success(*status);
            return;
          }
          if (error) {
            error("IoT Core response is missing a valid notification_status");
          }
          return;
        }
        const int status = response ? response->statusCode : 0;
        if (error) {
          std::string message;
          if (response) {
            if (const auto *responseMessage =
                    iot::http::stringField(response->jsonBody, "message");
                response->jsonParsed && responseMessage != nullptr &&
                !responseMessage->empty()) {
              message = ": " + *responseMessage;
            }
          }
          error("Failed to send to iot_core. HTTP Status: " +
                std::to_string(status) + message);
        }
      });
}

void IoTClient::getTelemetry(
    int deviceId, std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  iot::http::Request request;
  request.method = iot::http::Method::Post;
  request.path = "/telemetry/query";
  request.jsonBody = {{"device_id", static_cast<std::int64_t>(deviceId)}};

  transport_->send(
      request,
      [success = std::move(success), error = std::move(error)](
          iot::http::Result result,
          std::optional<iot::http::Response> response) {
        if (result == iot::http::Result::Ok && response &&
            response->statusCode == 200) {
          if (success) success(response->body);
          return;
        }
        const int status = response ? response->statusCode : 0;
        if (error) {
          error("Failed to get telemetry from iot_core. HTTP Status: " +
                std::to_string(status));
        }
      });
}

void IoTClient::deleteTelemetry(
    int deviceId, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  iot::http::Request request;
  request.method = iot::http::Method::Delete;
  request.path = "/telemetry";
  request.jsonBody = {{"device_id", static_cast<std::int64_t>(deviceId)}};

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
          error("Failed to delete telemetry from iot_core. HTTP Status: " +
                std::to_string(status));
        }
      });
}
