#include "TestFramework.h"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "clients/ITelegramApi.h"
#include "clients/TelegramBotClient.h"
#include "clients/WorkerThreadPool.h"
#include "common/http/HttpTransport.h"
#include "ingest/clients/IoTClient.h"
#include "clients/NotificationClient.h"
#include "telemetry/clients/RuleEngineClient.h"

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
    if (completion)
      completion(nextResult, nextResponse);
  }
};

iot::http::Response response(int status, std::string body,
                             iot::http::JsonObject parsed = {}) {
  return iot::http::Response{
      status, std::move(body), true, std::move(parsed), {}};
}

const iot::http::Request &lastRequest(const FakeHttpTransport &transport) {
  return transport.requests.back();
}

class FakeTelegramApi final : public ITelegramApi {
public:
  bool enabled{true};
  std::string rejection;
  mutable std::string sentChatId;
  mutable std::string sentText;
  mutable std::string sentParseMode;

  bool isEnabled() const override { return enabled; }

  void sendMessage(const std::string &chatId, const std::string &text,
                   const std::string &parseMode) const override {
    if (!rejection.empty()) {
      throw std::runtime_error(rejection);
    }
    sentChatId = chatId;
    sentText = text;
    sentParseMode = parseMode;
  }
};

} // namespace

// IoTClient: проверяем не fake-объект сервиса, а настоящий код HTTP-клиента.
TEST_CASE(iot_client_send_uses_post_telemetry_and_correct_json_fields) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(
      200, R"({"notification_status":"not_triggered"})",
      {{"notification_status", std::string("not_triggered")}});
  IoTClient client(transport);

  bool succeeded = false;
  std::string notificationStatus;
  std::string errorMessage;
  client.sendTelemetry(
      42, 23.75,
      [&](const std::string &status) {
        succeeded = true;
        notificationStatus = status;
      },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_TRUE(succeeded);
  EXPECT_EQ("not_triggered", notificationStatus);
  EXPECT_TRUE(errorMessage.empty());
  EXPECT_EQ(1U, transport->requests.size());
  EXPECT_EQ(iot::http::Method::Post, lastRequest(*transport).method);
  EXPECT_EQ(std::string("/telemetry"), lastRequest(*transport).path);
  EXPECT_EQ(
      std::int64_t(42),
      std::get<std::int64_t>(lastRequest(*transport).jsonBody.at("device_id")));
  EXPECT_EQ(23.75, std::get<double>(
                       lastRequest(*transport).jsonBody.at("temperature")));
}

TEST_CASE(iot_client_send_reports_missing_notification_status) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(200, "{}");
  IoTClient client(transport);

  bool succeeded = false;
  std::string errorMessage;
  client.sendTelemetry(
      42, 23.75, [&](const std::string &) { succeeded = true; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_EQ("IoT Core response is missing a valid notification_status",
            errorMessage);
}

TEST_CASE(iot_client_send_reports_non_200_http_status) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(503, "service unavailable");
  IoTClient client(transport);

  bool succeeded = false;
  std::string errorMessage;
  client.sendTelemetry(
      7, 18.0, [&](const std::string &) { succeeded = true; },
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
  EXPECT_EQ(
      std::int64_t(19),
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
  EXPECT_EQ(
      std::int64_t(5),
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

// RuleEngineClient.
TEST_CASE(rule_engine_client_posts_telemetry_and_returns_rule_status) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(
      200, R"({"status":"ok","notification_status":"telegram=sent; email=sent"})",
      {{"status", std::string("ok")},
       {"notification_status", std::string("telegram=sent; email=sent")}});
  RuleEngineClient client("http://rule-engine", transport);

  std::string notificationStatus;
  std::string errorMessage;
  client.checkTelemetry(
      12, 31.5, [&](const std::string &status) { notificationStatus = status; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_EQ("telegram=sent; email=sent", notificationStatus);
  EXPECT_TRUE(errorMessage.empty());
  EXPECT_EQ(iot::http::Method::Post, lastRequest(*transport).method);
  EXPECT_EQ(std::string("/telemetry"),
            lastRequest(*transport).path);
  EXPECT_EQ(
      std::int64_t(12),
      std::get<std::int64_t>(lastRequest(*transport).jsonBody.at("device_id")));
  EXPECT_EQ(31.5, std::get<double>(
                      lastRequest(*transport).jsonBody.at("temperature")));
}

TEST_CASE(rule_engine_client_uses_error_message_from_service_response) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse =
      response(500, "{\"message\":\"notification failed\"}",
               {{"message", std::string("notification failed")}});
  RuleEngineClient client("http://rule-engine", transport);

  bool succeeded = false;
  std::string errorMessage;
  client.checkTelemetry(
      12, 31.5, [&](const std::string &) { succeeded = true; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("500") != std::string::npos);
  EXPECT_TRUE(errorMessage.find("notification failed") != std::string::npos);
}

TEST_CASE(rule_engine_client_rejects_empty_base_url_without_sending) {
  auto transport = std::make_shared<FakeHttpTransport>();
  RuleEngineClient client("", transport);

  std::string errorMessage;
  client.checkTelemetry(
      1, 80.0, [](const std::string &) {},
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_EQ(0U, transport->requests.size());
  EXPECT_TRUE(errorMessage.find("not configured") != std::string::npos);
}

TEST_CASE(rule_engine_client_reports_generic_http_error) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(502, "not-json");
  transport->nextResponse->jsonParsed = false;
  RuleEngineClient client("http://rule-engine", transport);

  std::string errorMessage;
  client.checkTelemetry(
      1, 80.0, [](const std::string &) {},
      [&](const std::string &error) { errorMessage = error; });
  EXPECT_TRUE(errorMessage.find("HTTP 502") != std::string::npos);
}

TEST_CASE(notification_client_forwards_device_and_all_notification_recipients) {
  auto transport = std::make_shared<FakeHttpTransport>();
  transport->nextResponse = response(
      200, R"({"status":"ok","telegram":"sent","email":"sent"})",
      {{"status", std::string("ok")},
       {"telegram", std::string("sent")},
       {"email", std::string("sent")}});
  NotificationClient client("http://notification-service", transport);

  std::string statuses;
  std::string errorMessage;
  client.sendTemperatureAlert(
      42, "chat-42", "owner@example.com", "Boiler", 81.5, 70.0,
      [&](const std::string &value) { statuses = value; },
      [&](const std::string &value) { errorMessage = value; });

  EXPECT_EQ("telegram=sent; email=sent", statuses);
  EXPECT_TRUE(errorMessage.empty());
  EXPECT_EQ(iot::http::Method::Post, lastRequest(*transport).method);
  EXPECT_EQ("/notifications/temperature", lastRequest(*transport).path);
  EXPECT_EQ(std::int64_t(42),
            std::get<std::int64_t>(
                lastRequest(*transport).jsonBody.at("device_id")));
  EXPECT_EQ("chat-42",
            std::get<std::string>(
                lastRequest(*transport).jsonBody.at("chat_id")));
  EXPECT_EQ("owner@example.com",
            std::get<std::string>(lastRequest(*transport).jsonBody.at("email")));
}

TEST_CASE(notification_client_resets_deduplication_for_device) {
  auto transport = std::make_shared<FakeHttpTransport>();
  NotificationClient client("http://notification-service", transport);
  bool succeeded = false;
  std::string errorMessage;

  client.clearTemperatureAlert(
      42, [&] { succeeded = true; },
      [&](const std::string &value) { errorMessage = value; });

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(errorMessage.empty());
  EXPECT_EQ(iot::http::Method::Post, lastRequest(*transport).method);
  EXPECT_EQ("/notifications/temperature/normal", lastRequest(*transport).path);
  EXPECT_EQ(std::int64_t(42),
            std::get<std::int64_t>(
                lastRequest(*transport).jsonBody.at("device_id")));
}

// TelegramBotClient.
TEST_CASE(telegram_client_sends_through_api_on_worker_pool) {
  auto api = std::make_shared<FakeTelegramApi>();
  auto threadPool = std::make_shared<WorkerThreadPool>(1);
  TelegramBotClient client(api, threadPool);

  bool succeeded = false;
  std::string errorMessage;
  std::mutex mutex;
  std::condition_variable condition;
  bool completed = false;
  const auto finish = [&] {
    {
      std::lock_guard lock(mutex);
      completed = true;
    }
    condition.notify_one();
  };
  client.sendMessage(
      "chat-7", "<b>Temperature is high</b>", "HTML",
      [&] {
        succeeded = true;
        finish();
      },
      [&](const std::string &error) {
        errorMessage = error;
        finish();
      });

  std::unique_lock lock(mutex);
  const bool finished = condition.wait_for(lock, std::chrono::seconds(5),
                                           [&] { return completed; });
  lock.unlock();
  EXPECT_TRUE(finished);
  if (!finished)
    return;
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(errorMessage.empty());
  EXPECT_EQ(std::string("chat-7"), api->sentChatId);
  EXPECT_EQ(std::string("<b>Temperature is high</b>"), api->sentText);
  EXPECT_EQ(std::string("HTML"), api->sentParseMode);
}

TEST_CASE(telegram_client_reports_api_exception) {
  auto api = std::make_shared<FakeTelegramApi>();
  api->rejection = "chat not found";
  auto threadPool = std::make_shared<WorkerThreadPool>(1);
  TelegramBotClient client(api, threadPool);
  bool succeeded = false;
  std::string errorMessage;
  std::mutex mutex;
  std::condition_variable condition;
  bool completed = false;
  client.sendMessage(
      "bad-chat", "hello", [&] { succeeded = true; },
      [&](const std::string &error) {
        errorMessage = error;
        {
          std::lock_guard lock(mutex);
          completed = true;
        }
        condition.notify_one();
      });

  std::unique_lock lock(mutex);
  const bool finished = condition.wait_for(lock, std::chrono::seconds(5),
                                           [&] { return completed; });
  lock.unlock();
  EXPECT_TRUE(finished);
  if (!finished)
    return;
  EXPECT_FALSE(succeeded);
  EXPECT_TRUE(errorMessage.find("chat not found") != std::string::npos);
}

TEST_CASE(telegram_client_rejects_messages_when_not_configured) {
  auto api = std::make_shared<FakeTelegramApi>();
  api->enabled = false;
  auto threadPool = std::make_shared<WorkerThreadPool>(1);
  TelegramBotClient client(api, threadPool);
  bool succeeded = false;
  std::string errorMessage;
  client.sendMessage(
      "chat-7", "hello", [&] { succeeded = true; },
      [&](const std::string &error) { errorMessage = error; });

  EXPECT_TRUE(errorMessage.find("not configured") != std::string::npos);
}
