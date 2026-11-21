#include "TestFramework.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <fakeit.hpp>
#include "HttpTransportMock.h"

#include "common/http/HttpTransport.h"
#include "ingest/clients/IoTClient.h"
#include "telemetry/clients/NotificationServiceClient.h"
#include "clients/TelegramBotClient.h"

namespace {
iot::http::Response response(
    int status, std::string body, iot::http::JsonObject parsed = {}) {
  return iot::http::Response{status, std::move(body), true,
                             std::move(parsed), {}};
}
}  // namespace

// IoTClient: проверяем не fake-объект сервиса, а настоящий код HTTP-клиента.
TEST_CASE(iot_client_send_uses_post_telemetry_and_correct_json_fields) {
  test::HttpTransportMock transport;
  transport.response() = response(201, "{}");
  IoTClient client(transport.shared());

  bool succeeded = false;
  std::string errorMessage;
  client.sendTelemetry(
      42, 23.75, [&] { succeeded = true; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(errorMessage.empty());
  EXPECT_EQ(1U, transport.requests().size());
  EXPECT_EQ(iot::http::Method::Post, transport.lastRequest().method);
  EXPECT_EQ(std::string("/devices/42/telemetry"), transport.lastRequest().path);
  EXPECT_TRUE(transport.lastRequest().jsonBody.find("device_id") ==
              transport.lastRequest().jsonBody.end());
  EXPECT_EQ(23.75,
            std::get<double>(transport.lastRequest().jsonBody.at("temperature")));
  fakeit::Verify(Method(transport.mock(), send)).Once();
}

TEST_CASE(iot_client_send_reports_non_200_http_status) {
  test::HttpTransportMock transport;
  transport.response() = response(503, "service unavailable");
  IoTClient client(transport.shared());

  bool succeeded = false;
  std::string errorMessage;
  client.sendTelemetry(7, 18.0, [&] { succeeded = true; },
                       [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("503") != std::string::npos);
  fakeit::Verify(Method(transport.mock(), send)).Once();
}

TEST_CASE(iot_client_get_uses_query_endpoint_and_returns_response_body) {
  test::HttpTransportMock transport;
  transport.response() = response(200, "[{\"temperature\":21.5}]");
  IoTClient client(transport.shared());

  std::string received;
  std::string errorMessage;
  client.getTelemetry(
      19, [&](const std::string &body) { received = body; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_EQ(iot::http::Method::Get, transport.lastRequest().method);
  EXPECT_EQ(std::string("/devices/19/telemetry"), transport.lastRequest().path);
  EXPECT_TRUE(transport.lastRequest().jsonBody.empty());
  EXPECT_EQ(std::string("[{\"temperature\":21.5}]"), received);
  EXPECT_TRUE(errorMessage.empty());
  fakeit::Verify(Method(transport.mock(), send)).Once();
}

TEST_CASE(iot_client_delete_uses_delete_telemetry_and_device_id) {
  test::HttpTransportMock transport;
  transport.response() = response(204, "");
  IoTClient client(transport.shared());

  bool succeeded = false;
  std::string errorMessage;
  client.deleteTelemetry(
      5, [&] { succeeded = true; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(errorMessage.empty());
  EXPECT_EQ(iot::http::Method::Delete, transport.lastRequest().method);
  EXPECT_EQ(std::string("/devices/5/telemetry"), transport.lastRequest().path);
  EXPECT_TRUE(transport.lastRequest().jsonBody.empty());
  fakeit::Verify(Method(transport.mock(), send)).Once();
}

TEST_CASE(iot_client_reports_network_failure_without_response) {
  test::HttpTransportMock transport;
  transport.result() = iot::http::Result::NetworkError;
  transport.response() = std::nullopt;
  IoTClient client(transport.shared());

  bool succeeded = false;
  std::string errorMessage;
  client.getTelemetry(
      1, [&](const std::string &) { succeeded = true; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("HTTP Status: 0") != std::string::npos);
  fakeit::Verify(Method(transport.mock(), send)).Once();
}

// NotificationServiceClient.
TEST_CASE(notification_client_posts_expected_payload_and_handles_success) {
  test::HttpTransportMock transport;
  transport.response() = response(200, "{\"status\":\"ok\"}");
  NotificationServiceClient client("http://notification-service", transport.shared());

  bool succeeded = false;
  std::string errorMessage;
  client.sendTemperatureAlert(
      "chat-123", "Boiler", 81.5, 70.0, [&] { succeeded = true; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(errorMessage.empty());
  EXPECT_EQ(iot::http::Method::Post, transport.lastRequest().method);
  EXPECT_EQ(std::string("/notifications/temperature-alerts"),
            transport.lastRequest().path);
  EXPECT_EQ(std::string("chat-123"),
            std::get<std::string>(transport.lastRequest().jsonBody.at("chat_id")));
  EXPECT_EQ(std::string("Boiler"),
            std::get<std::string>(transport.lastRequest().jsonBody.at("device_name")));
  EXPECT_EQ(81.5,
            std::get<double>(transport.lastRequest().jsonBody.at("temperature")));
  EXPECT_EQ(70.0,
            std::get<double>(transport.lastRequest().jsonBody.at("threshold")));
  fakeit::Verify(Method(transport.mock(), send)).Once();
}

TEST_CASE(notification_client_uses_error_message_from_service_response) {
  test::HttpTransportMock transport;
  transport.response() = response(
      400, "{\"message\":\"invalid chat id\"}",
      {{"message", std::string("invalid chat id")}});
  NotificationServiceClient client("http://notification-service", transport.shared());

  bool succeeded = false;
  std::string errorMessage;
  client.sendTemperatureAlert(
      "", "Boiler", 81.5, 70.0, [&] { succeeded = true; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("400") != std::string::npos);
  EXPECT_TRUE(errorMessage.find("invalid chat id") != std::string::npos);
  fakeit::Verify(Method(transport.mock(), send)).Once();
}

TEST_CASE(notification_client_rejects_empty_base_url_without_sending) {
  test::HttpTransportMock transport;
  NotificationServiceClient client("", transport.shared());

  std::string errorMessage;
  client.sendTemperatureAlert("1", "Boiler", 80.0, 70.0, [] {},
                              [&](const std::string &error) {
                                errorMessage = error;
                              });

  EXPECT_EQ(0U, transport.requests().size());
  EXPECT_TRUE(errorMessage.find("not configured") != std::string::npos);
  fakeit::Verify(Method(transport.mock(), send)).Never();
}

TEST_CASE(notification_client_reports_generic_http_error) {
  test::HttpTransportMock transport;
  transport.response() = response(502, "not-json");
  transport.response()->jsonParsed = false;
  NotificationServiceClient client("http://notification-service", transport.shared());

  std::string errorMessage;
  client.sendTemperatureAlert("1", "Boiler", 80.0, 70.0, [] {},
                              [&](const std::string &error) {
                                errorMessage = error;
                              });
  EXPECT_TRUE(errorMessage.find("HTTP 502") != std::string::npos);
  fakeit::Verify(Method(transport.mock(), send)).Once();
}

// TelegramBotClient.
TEST_CASE(telegram_client_uses_token_path_and_expected_json_payload) {
  test::HttpTransportMock transport;
  transport.response() = response(200, "{\"ok\":true}", {{"ok", true}});
  TelegramBotClient client("123:secret", transport.shared());

  bool succeeded = false;
  std::string errorMessage;
  client.sendMessage("chat-7", "Temperature is high", [&] { succeeded = true; },
                     [&](const std::string &error) { errorMessage = error; });

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(errorMessage.empty());
  EXPECT_EQ(iot::http::Method::Post, transport.lastRequest().method);
  EXPECT_EQ(std::string("/bot123:secret/sendMessage"),
            transport.lastRequest().path);
  EXPECT_EQ(std::string("chat-7"),
            std::get<std::string>(transport.lastRequest().jsonBody.at("chat_id")));
  EXPECT_EQ(std::string("Temperature is high"),
            std::get<std::string>(transport.lastRequest().jsonBody.at("text")));
  fakeit::Verify(Method(transport.mock(), send)).Once();
}

TEST_CASE(telegram_client_propagates_api_rejection_description) {
  test::HttpTransportMock transport;
  transport.response() = response(
      400, "{\"ok\":false,\"description\":\"chat not found\"}",
      {{"ok", false}, {"description", std::string("chat not found")}});
  TelegramBotClient client("123:secret", transport.shared());

  bool succeeded = false;
  std::string errorMessage;
  client.sendMessage("bad-chat", "hello", [&] { succeeded = true; },
                     [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("chat not found") != std::string::npos);
  fakeit::Verify(Method(transport.mock(), send)).Once();
}

TEST_CASE(telegram_client_rejects_unparseable_success_response) {
  test::HttpTransportMock transport;
  transport.response() = response(200, "not-json");
  transport.response()->jsonParsed = false;
  transport.response()->jsonParseError = "invalid JSON";
  TelegramBotClient client("123:secret", transport.shared());

  bool succeeded = false;
  std::string errorMessage;
  client.sendMessage("chat-7", "hello", [&] { succeeded = true; },
                     [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("Could not parse") != std::string::npos);
  fakeit::Verify(Method(transport.mock(), send)).Once();
}

TEST_CASE(telegram_client_reports_network_error) {
  test::HttpTransportMock transport;
  transport.result() = iot::http::Result::NetworkError;
  transport.response() = std::nullopt;
  TelegramBotClient client("123:secret", transport.shared());

  bool succeeded = false;
  std::string errorMessage;
  client.sendMessage("chat-7", "hello", [&] { succeeded = true; },
                     [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("HTTP 0") != std::string::npos);
  fakeit::Verify(Method(transport.mock(), send)).Once();
}

TEST_CASE(telegram_client_does_not_accept_ok_true_on_non_200_status) {
  test::HttpTransportMock transport;
  transport.response() = response(500, "{\"ok\":true}", {{"ok", true}});
  TelegramBotClient client("123:secret", transport.shared());

  bool succeeded = false;
  std::string errorMessage;
  client.sendMessage("chat-7", "hello", [&] { succeeded = true; },
                     [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("HTTP 500") != std::string::npos);
  fakeit::Verify(Method(transport.mock(), send)).Once();
}
