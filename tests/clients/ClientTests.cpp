#include "TestFramework.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "common/http/HttpTransport.h"
#include "ingest/clients/IoTClient.h"
#include "telemetry/clients/NotificationServiceClient.h"
#include "clients/TelegramBotClient.h"

namespace {

class FakeHttpTransport final : public iot::http::IHttpTransport {
 public:
  mutable std::vector<iot::http::Request> requests;
  iot::http::Result nextResult{iot::http::Result::Ok};
  std::optional<iot::http::Response> nextResponse{
      iot::http::Response{200, "{}", true, {}, {}}};

  void send(const iot::http::Request &request,
            iot::http::Completion completion) const override {
    requests.push_back(request);
    if (completion) completion(nextResult, nextResponse);
  }
};

iot::http::Response response(
    int status, std::string body, iot::http::JsonObject parsed = {}) {
  return iot::http::Response{status, std::move(body), true,
                             std::move(parsed), {}};
}

const iot::http::Request &lastRequest(const FakeHttpTransport &transport) {
  return transport.requests.back();
}

}  // namespace

// IoTClient: проверяем не fake-объект сервиса, а настоящий код HTTP-клиента.
TEST_CASE(iot_client_send_uses_post_telemetry_and_correct_json_fields) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(200, "{}");
  IoTClient client(transport);

  bool succeeded = false;
  std::string errorMessage;
  client.sendTelemetry(
      42, 23.75, [&] { succeeded = true; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(errorMessage.empty());
  EXPECT_EQ(1U, transport->requests.size());
  EXPECT_EQ(iot::http::Method::Post, lastRequest(*transport).method);
  EXPECT_EQ(std::string("/telemetry"), lastRequest(*transport).path);
  EXPECT_EQ(std::int64_t(42),
            std::get<std::int64_t>(lastRequest(*transport).jsonBody.at("device_id")));
  EXPECT_EQ(23.75,
            std::get<double>(lastRequest(*transport).jsonBody.at("temperature")));
}

TEST_CASE(iot_client_send_reports_non_200_http_status) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(503, "service unavailable");
  IoTClient client(transport);

  bool succeeded = false;
  std::string errorMessage;
  client.sendTelemetry(7, 18.0, [&] { succeeded = true; },
                       [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("503") != std::string::npos);
}

TEST_CASE(iot_client_get_uses_query_endpoint_and_returns_response_body) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(200, "[{\"temperature\":21.5}]");
  IoTClient client(transport);

  std::string received;
  std::string errorMessage;
  client.getTelemetry(
      19, [&](const std::string &body) { received = body; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_EQ(iot::http::Method::Post, lastRequest(*transport).method);
  EXPECT_EQ(std::string("/telemetry/query"), lastRequest(*transport).path);
  EXPECT_EQ(std::int64_t(19),
            std::get<std::int64_t>(lastRequest(*transport).jsonBody.at("device_id")));
  EXPECT_EQ(std::string("[{\"temperature\":21.5}]"), received);
  EXPECT_TRUE(errorMessage.empty());
}

TEST_CASE(iot_client_delete_uses_delete_telemetry_and_device_id) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(200, "{}");
  IoTClient client(transport);

  bool succeeded = false;
  std::string errorMessage;
  client.deleteTelemetry(
      5, [&] { succeeded = true; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(errorMessage.empty());
  EXPECT_EQ(iot::http::Method::Delete, lastRequest(*transport).method);
  EXPECT_EQ(std::string("/telemetry"), lastRequest(*transport).path);
  EXPECT_EQ(std::int64_t(5),
            std::get<std::int64_t>(lastRequest(*transport).jsonBody.at("device_id")));
}

TEST_CASE(iot_client_reports_network_failure_without_response) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResult = iot::http::Result::NetworkError;
  transport->nextResponse = std::nullopt;
  IoTClient client(transport);

  bool succeeded = false;
  std::string errorMessage;
  client.getTelemetry(
      1, [&](const std::string &) { succeeded = true; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("HTTP Status: 0") != std::string::npos);
}

// NotificationServiceClient.
TEST_CASE(notification_client_posts_expected_payload_and_handles_success) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(200, "{\"status\":\"ok\"}");
  NotificationServiceClient client("http://notification-service", transport);

  bool succeeded = false;
  std::string errorMessage;
  client.sendTemperatureAlert(
      "chat-123", "Boiler", 81.5, 70.0, [&] { succeeded = true; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(errorMessage.empty());
  EXPECT_EQ(iot::http::Method::Post, lastRequest(*transport).method);
  EXPECT_EQ(std::string("/notifications/temperature"),
            lastRequest(*transport).path);
  EXPECT_EQ(std::string("chat-123"),
            std::get<std::string>(lastRequest(*transport).jsonBody.at("chat_id")));
  EXPECT_EQ(std::string("Boiler"),
            std::get<std::string>(lastRequest(*transport).jsonBody.at("device_name")));
  EXPECT_EQ(81.5,
            std::get<double>(lastRequest(*transport).jsonBody.at("temperature")));
  EXPECT_EQ(70.0,
            std::get<double>(lastRequest(*transport).jsonBody.at("threshold")));
}

TEST_CASE(notification_client_uses_error_message_from_service_response) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(
      400, "{\"message\":\"invalid chat id\"}",
      {{"message", std::string("invalid chat id")}});
  NotificationServiceClient client("http://notification-service", transport);

  bool succeeded = false;
  std::string errorMessage;
  client.sendTemperatureAlert(
      "", "Boiler", 81.5, 70.0, [&] { succeeded = true; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("400") != std::string::npos);
  EXPECT_TRUE(errorMessage.find("invalid chat id") != std::string::npos);
}

TEST_CASE(notification_client_rejects_empty_base_url_without_sending) {
  auto transport = std::make_shared<FakeHttpTransport>();
  NotificationServiceClient client("", transport);

  std::string errorMessage;
  client.sendTemperatureAlert("1", "Boiler", 80.0, 70.0, [] {},
                              [&](const std::string &error) {
                                errorMessage = error;
                              });

  EXPECT_EQ(0U, transport->requests.size());
  EXPECT_TRUE(errorMessage.find("not configured") != std::string::npos);
}

TEST_CASE(notification_client_reports_generic_http_error) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(502, "not-json");
  transport->nextResponse->jsonParsed = false;
  NotificationServiceClient client("http://notification-service", transport);

  std::string errorMessage;
  client.sendTemperatureAlert("1", "Boiler", 80.0, 70.0, [] {},
                              [&](const std::string &error) {
                                errorMessage = error;
                              });
  EXPECT_TRUE(errorMessage.find("HTTP 502") != std::string::npos);
}

// TelegramBotClient.
TEST_CASE(telegram_client_uses_token_path_and_expected_json_payload) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(200, "{\"ok\":true}", {{"ok", true}});
  TelegramBotClient client("123:secret", transport);

  bool succeeded = false;
  std::string errorMessage;
  client.sendMessage("chat-7", "Temperature is high", [&] { succeeded = true; },
                     [&](const std::string &error) { errorMessage = error; });

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(errorMessage.empty());
  EXPECT_EQ(iot::http::Method::Post, lastRequest(*transport).method);
  EXPECT_EQ(std::string("/bot123:secret/sendMessage"),
            lastRequest(*transport).path);
  EXPECT_EQ(std::string("chat-7"),
            std::get<std::string>(lastRequest(*transport).jsonBody.at("chat_id")));
  EXPECT_EQ(std::string("Temperature is high"),
            std::get<std::string>(lastRequest(*transport).jsonBody.at("text")));
}

TEST_CASE(telegram_client_propagates_api_rejection_description) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(
      400, "{\"ok\":false,\"description\":\"chat not found\"}",
      {{"ok", false}, {"description", std::string("chat not found")}});
  TelegramBotClient client("123:secret", transport);

  bool succeeded = false;
  std::string errorMessage;
  client.sendMessage("bad-chat", "hello", [&] { succeeded = true; },
                     [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("chat not found") != std::string::npos);
}

TEST_CASE(telegram_client_rejects_unparseable_success_response) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(200, "not-json");
  transport->nextResponse->jsonParsed = false;
  transport->nextResponse->jsonParseError = "invalid JSON";
  TelegramBotClient client("123:secret", transport);

  bool succeeded = false;
  std::string errorMessage;
  client.sendMessage("chat-7", "hello", [&] { succeeded = true; },
                     [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("Could not parse") != std::string::npos);
}

TEST_CASE(telegram_client_reports_network_error) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResult = iot::http::Result::NetworkError;
  transport->nextResponse = std::nullopt;
  TelegramBotClient client("123:secret", transport);

  bool succeeded = false;
  std::string errorMessage;
  client.sendMessage("chat-7", "hello", [&] { succeeded = true; },
                     [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("HTTP 0") != std::string::npos);
}

TEST_CASE(telegram_client_does_not_accept_ok_true_on_non_200_status) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(500, "{\"ok\":true}", {{"ok", true}});
  TelegramBotClient client("123:secret", transport);

  bool succeeded = false;
  std::string errorMessage;
  client.sendMessage("chat-7", "hello", [&] { succeeded = true; },
                     [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("HTTP 500") != std::string::npos);
}
