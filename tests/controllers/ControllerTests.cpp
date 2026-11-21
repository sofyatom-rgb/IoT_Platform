#include "TestFramework.h"
#include "ControllerTestSupport.h"

#include <drogon/drogon.h>

#include <string>

#include "controllers/NotificationController.h"
#include "devices/controllers/DevicesController.h"
#include "ingest/controllers/IngestController.h"
#include "telemetry/controllers/TelemetryController.h"
#include "users/controllers/UsersController.h"

namespace {

/// Возвращает HTTP-статус ответа контроллера.
/// Возвращает числовой HTTP-статус ответа.
int statusCode(const drogon::HttpResponsePtr &response) {
  return static_cast<int>(response->getStatusCode());
}

/// Создаёт Drogon-запрос с JSON-телом.
/// Создаёт HTTP-запрос с JSON-телом.
drogon::HttpRequestPtr jsonRequest(const std::string &body) {
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  request->setBody(body);
  return request;
}

}  // namespace

TEST_CASE(users_controller_create_forwards_valid_payload_and_returns_user_id) {
  auto service = std::make_shared<test_support::UsersServiceFixture>();
  UsersController controller(service->shared());
  auto request = jsonRequest(
      R"({"email":"a@example.com","password":"secret","name":"Alice","telegram_chat_id":"-100123"})");

  drogon::HttpResponsePtr response;
  controller.create(request, [&](const auto &value) { response = value; });

  EXPECT_TRUE(static_cast<bool>(response));
  EXPECT_EQ(static_cast<int>(drogon::k200OK), statusCode(response));
  EXPECT_EQ(1, service->saveCalls);
  EXPECT_EQ(std::string("a@example.com"), service->email);
  EXPECT_EQ(std::string("secret"), service->password);
  EXPECT_EQ(std::string("Alice"), service->name);
  EXPECT_EQ(std::string("-100123"), service->chatId);
}

TEST_CASE(users_controller_rejects_invalid_telegram_chat_id_before_service_call) {
  auto service = std::make_shared<test_support::UsersServiceFixture>();
  UsersController controller(service->shared());
  auto request = jsonRequest(
      R"({"email":"a@example.com","password":"secret","name":"Alice","telegram_chat_id":"chat-not-number"})");

  drogon::HttpResponsePtr response;
  controller.create(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, service->saveCalls);
}

TEST_CASE(users_controller_rejects_invalid_json_with_bad_request) {
  auto service = std::make_shared<test_support::UsersServiceFixture>();
  UsersController controller(service->shared());
  auto request = jsonRequest("{not-json");

  drogon::HttpResponsePtr response;
  controller.create(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, service->saveCalls);
}

TEST_CASE(users_controller_rejects_non_positive_delete_id) {
  auto service = std::make_shared<test_support::UsersServiceFixture>();
  UsersController controller(service->shared());
  auto request = jsonRequest(R"({"user_id":0})");

  drogon::HttpResponsePtr response;
  controller.deleteUser(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, service->deleteCalls);
}

TEST_CASE(users_controller_update_rejects_partial_or_non_numeric_id_without_throwing) {
  auto service = std::make_shared<test_support::UsersServiceFixture>();
  UsersController controller(service->shared());
  auto request = jsonRequest(R"({"email":"a@example.com","password":"x","name":"A"})");
  request->setParameter("id", "12oops");

  drogon::HttpResponsePtr response;
  controller.updateUser(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, service->updateCalls);
}

TEST_CASE(users_controller_get_forwards_valid_id) {
  auto service = std::make_shared<test_support::UsersServiceFixture>();
  UsersController controller(service->shared());
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setParameter("id", "42");

  drogon::HttpResponsePtr response;
  controller.getUser(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(42, service->requestedId);
  EXPECT_EQ(1, service->getCalls);
  EXPECT_EQ(static_cast<int>(drogon::k200OK), statusCode(response));
}

TEST_CASE(users_controller_get_returns_not_found_when_service_reports_missing_user) {
  auto service = std::make_shared<test_support::UsersServiceFixture>();
  service->getResponse = R"({"status":"error","message":"User not found"})";
  UsersController controller(service->shared());
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setParameter("id", "999999");
  drogon::HttpResponsePtr response;
  controller.getUser(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k404NotFound), statusCode(response));
}

TEST_CASE(users_controller_get_returns_server_error_for_invalid_service_json) {
  auto service = std::make_shared<test_support::UsersServiceFixture>();
  service->getResponse = "not-json";
  UsersController controller(service->shared());
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setParameter("id", "42");
  drogon::HttpResponsePtr response;
  controller.getUser(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k500InternalServerError), statusCode(response));
}

TEST_CASE(devices_controller_create_rejects_missing_required_fields) {
  auto service = std::make_shared<test_support::DevicesServiceFixture>();
  DevicesController controller(service->shared());
  auto request = jsonRequest(R"({"owner_id":0,"name":"","type":"sensor"})");

  drogon::HttpResponsePtr response;
  controller.create(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, service->saveCalls);
}

TEST_CASE(devices_controller_create_forwards_valid_device) {
  auto service = std::make_shared<test_support::DevicesServiceFixture>();
  DevicesController controller(service->shared());
  auto request = jsonRequest(R"({"owner_id":9,"name":"Boiler","type":"temperature"})");

  drogon::HttpResponsePtr response;
  controller.create(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(1, service->saveCalls);
  EXPECT_EQ(9, service->ownerId);
  EXPECT_EQ(std::string("Boiler"), service->name);
  EXPECT_EQ(std::string("temperature"), service->type);
  EXPECT_EQ(static_cast<int>(drogon::k200OK), statusCode(response));
}

TEST_CASE(devices_controller_update_rejects_invalid_id_without_throwing) {
  auto service = std::make_shared<test_support::DevicesServiceFixture>();
  DevicesController controller(service->shared());
  auto request = jsonRequest(R"({"owner_id":9,"name":"Boiler","type":"temperature"})");
  request->setParameter("id", "1x");

  drogon::HttpResponsePtr response;
  controller.updateDevice(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, service->updateCalls);
}

TEST_CASE(devices_controller_delete_rejects_negative_id) {
  auto service = std::make_shared<test_support::DevicesServiceFixture>();
  DevicesController controller(service->shared());
  auto request = jsonRequest(R"({"device_id":-3})");

  drogon::HttpResponsePtr response;
  controller.deleteDevice(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, service->deleteCalls);
}

TEST_CASE(devices_controller_get_returns_not_found_when_service_reports_missing_device) {
  auto service = std::make_shared<test_support::DevicesServiceFixture>();
  service->getResponse = R"({"status":"error","message":"Device not found"})";
  DevicesController controller(service->shared());
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setParameter("id", "999999");
  drogon::HttpResponsePtr response;
  controller.getDevice(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k404NotFound), statusCode(response));
}

TEST_CASE(devices_controller_get_returns_server_error_for_invalid_service_json) {
  auto service = std::make_shared<test_support::DevicesServiceFixture>();
  service->getResponse = "not-json";
  DevicesController controller(service->shared());
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setParameter("id", "202");
  drogon::HttpResponsePtr response;
  controller.getDevice(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k500InternalServerError), statusCode(response));
}

TEST_CASE(telemetry_controller_create_rejects_invalid_device_id_and_temperature) {
  auto repository = std::make_shared<test_support::TelemetryRepositoryFixture>();
  TelemetryController controller(test_support::makeTelemetryService(repository));
  auto request = jsonRequest(R"({"device_id":0,"temperature":"hot"})");

  drogon::HttpResponsePtr response;
  controller.create(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, repository->saves);
}

TEST_CASE(telemetry_controller_create_forwards_valid_reading_to_service) {
  auto repository = std::make_shared<test_support::TelemetryRepositoryFixture>();
  TelemetryController controller(test_support::makeTelemetryService(repository));
  auto request = jsonRequest(R"({"device_id":17,"temperature":22.75})");

  drogon::HttpResponsePtr response;
  controller.create(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(1, repository->saves);
  EXPECT_EQ(17, repository->deviceId);
  EXPECT_EQ(22.75, repository->temperature);
  EXPECT_EQ(static_cast<int>(drogon::k200OK), statusCode(response));
}

TEST_CASE(telemetry_controller_read_rejects_non_positive_device_id) {
  auto repository = std::make_shared<test_support::TelemetryRepositoryFixture>();
  TelemetryController controller(test_support::makeTelemetryService(repository));
  auto request = jsonRequest(R"({"device_id":-1})");

  drogon::HttpResponsePtr response;
  controller.getTelemetry(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, repository->reads);
}

TEST_CASE(telemetry_controller_delete_forwards_valid_device_id) {
  auto repository = std::make_shared<test_support::TelemetryRepositoryFixture>();
  TelemetryController controller(test_support::makeTelemetryService(repository));
  auto request = jsonRequest(R"({"device_id":17})");

  drogon::HttpResponsePtr response;
  controller.deleteTelemetry(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(1, repository->deletes);
  EXPECT_EQ(17, repository->deviceId);
  EXPECT_EQ(static_cast<int>(drogon::k200OK), statusCode(response));
}

TEST_CASE(notification_controller_rejects_invalid_payload_with_bad_request) {
  auto telegram = std::make_shared<test_support::TelegramClientFixture>();
  NotificationController controller{NotificationService(telegram->shared())};
  auto request = jsonRequest(R"({"chat_id":"","device_name":"Boiler","temperature":80,"threshold":70})");

  drogon::HttpResponsePtr response;
  controller.sendTemperatureAlert(
      request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, telegram->calls);
}


TEST_CASE(notification_controller_forwards_valid_payload_to_notification_service) {
  auto telegram = std::make_shared<test_support::TelegramClientFixture>();
  NotificationController controller{NotificationService(telegram->shared())};
  auto request = jsonRequest(
      R"({"chat_id":"chat-44","device_name":"Boiler","temperature":81.5,"threshold":70})");

  drogon::HttpResponsePtr response;
  controller.sendTemperatureAlert(
      request, [&](const auto &value) { response = value; });

  EXPECT_EQ(1, telegram->calls);
  EXPECT_EQ(std::string("chat-44"), telegram->chatId);
  EXPECT_TRUE(telegram->message.find("Boiler") != std::string::npos);
  EXPECT_EQ(static_cast<int>(drogon::k200OK), statusCode(response));
}

TEST_CASE(notification_controller_maps_telegram_failure_to_bad_gateway) {
  auto telegram = std::make_shared<test_support::TelegramClientFixture>();
  telegram->failure = "Telegram API unavailable";
  NotificationController controller{NotificationService(telegram->shared())};
  auto request = jsonRequest(
      R"({"chat_id":"chat-44","device_name":"Boiler","temperature":81.5,"threshold":70})");

  drogon::HttpResponsePtr response;
  controller.sendTemperatureAlert(
      request, [&](const auto &value) { response = value; });

  EXPECT_EQ(1, telegram->calls);
  EXPECT_EQ(static_cast<int>(drogon::k502BadGateway), statusCode(response));
}

TEST_CASE(users_controller_create_rejects_whitespace_password) {
  auto service = std::make_shared<test_support::UsersServiceFixture>();
  UsersController controller(service->shared());
  auto request = jsonRequest(
      R"({"email":"a@example.com","password":"   ","name":"Alice"})");
  drogon::HttpResponsePtr response;
  controller.create(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, service->saveCalls);
}

TEST_CASE(users_controller_update_forwards_all_fields) {
  auto service = std::make_shared<test_support::UsersServiceFixture>();
  UsersController controller(service->shared());
  auto request = jsonRequest(
      R"({"email":"new@example.com","password":"secret2","name":"New Name","telegram_chat_id":12345})");
  request->setParameter("id", "77");
  drogon::HttpResponsePtr response;
  controller.updateUser(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k200OK), statusCode(response));
  EXPECT_EQ(1, service->updateCalls);
  EXPECT_EQ(77, service->requestedId);
  EXPECT_EQ(std::string("new@example.com"), service->email);
  EXPECT_EQ(std::string("secret2"), service->password);
  EXPECT_EQ(std::string("New Name"), service->name);
  EXPECT_EQ(std::string("12345"), service->chatId);
}

TEST_CASE(users_controller_maps_service_failure_to_internal_server_error) {
  auto service = std::make_shared<test_support::UsersServiceFixture>();
  service->failure = "database unavailable";
  UsersController controller(service->shared());
  auto request = jsonRequest(
      R"({"email":"a@example.com","password":"secret","name":"Alice"})");
  drogon::HttpResponsePtr response;
  controller.create(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k500InternalServerError), statusCode(response));
  EXPECT_EQ(1, service->saveCalls);
}

TEST_CASE(users_controller_get_rejects_invalid_id) {
  auto service = std::make_shared<test_support::UsersServiceFixture>();
  UsersController controller(service->shared());
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setParameter("id", "0");
  drogon::HttpResponsePtr response;
  controller.getUser(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, service->getCalls);
}

TEST_CASE(users_controller_maps_lookup_failure_to_internal_server_error) {
  auto service = std::make_shared<test_support::UsersServiceFixture>();
  service->failure = "database unavailable";
  UsersController controller(service->shared());
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setParameter("id", "42");
  drogon::HttpResponsePtr response;
  controller.getUser(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k500InternalServerError), statusCode(response));
  EXPECT_EQ(1, service->getCalls);
}

TEST_CASE(devices_controller_update_forwards_valid_fields) {
  auto service = std::make_shared<test_support::DevicesServiceFixture>();
  DevicesController controller(service->shared());
  auto request = jsonRequest(
      R"({"owner_id":11,"name":"Hall sensor","type":"temperature"})");
  request->setParameter("id", "25");
  drogon::HttpResponsePtr response;
  controller.updateDevice(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k200OK), statusCode(response));
  EXPECT_EQ(1, service->updateCalls);
  EXPECT_EQ(25, service->requestedId);
  EXPECT_EQ(11, service->ownerId);
  EXPECT_EQ(std::string("Hall sensor"), service->name);
}

TEST_CASE(devices_controller_get_forwards_valid_id_and_parses_json) {
  auto service = std::make_shared<test_support::DevicesServiceFixture>();
  DevicesController controller(service->shared());
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setParameter("id", "202");
  drogon::HttpResponsePtr response;
  controller.getDevice(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k200OK), statusCode(response));
  EXPECT_EQ(1, service->getCalls);
  EXPECT_EQ(202, service->requestedId);
}

TEST_CASE(devices_controller_maps_repository_failure_to_internal_server_error) {
  auto service = std::make_shared<test_support::DevicesServiceFixture>();
  service->failure = "database unavailable";
  DevicesController controller(service->shared());
  auto request = jsonRequest(
      R"({"owner_id":9,"name":"Boiler","type":"temperature"})");
  drogon::HttpResponsePtr response;
  controller.create(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k500InternalServerError), statusCode(response));
  EXPECT_EQ(1, service->saveCalls);
}

TEST_CASE(telemetry_controller_rejects_malformed_json) {
  auto repository = std::make_shared<test_support::TelemetryRepositoryFixture>();
  TelemetryController controller(test_support::makeTelemetryService(repository));
  auto request = jsonRequest("{bad-json");
  drogon::HttpResponsePtr response;
  controller.create(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, repository->saves);
}

TEST_CASE(telemetry_controller_read_returns_parsed_array) {
  auto repository = std::make_shared<test_support::TelemetryRepositoryFixture>();
  TelemetryController controller(test_support::makeTelemetryService(repository));
  auto request = jsonRequest(R"({"device_id":5})");
  drogon::HttpResponsePtr response;
  controller.getTelemetry(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k200OK), statusCode(response));
  EXPECT_EQ(1, repository->reads);
  EXPECT_EQ(5, repository->deviceId);
}

TEST_CASE(telemetry_controller_maps_repository_errors_to_internal_server_error) {
  auto repository = std::make_shared<test_support::TelemetryRepositoryFixture>();
  repository->failure = "database unavailable";
  TelemetryController controller(test_support::makeTelemetryService(repository));
  auto request = jsonRequest(R"({"device_id":5})");
  drogon::HttpResponsePtr response;
  controller.getTelemetry(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k500InternalServerError), statusCode(response));
  EXPECT_EQ(1, repository->reads);
}

TEST_CASE(telemetry_controller_delete_maps_repository_error_to_internal_server_error) {
  auto repository = std::make_shared<test_support::TelemetryRepositoryFixture>();
  repository->failure = "database unavailable";
  TelemetryController controller(test_support::makeTelemetryService(repository));
  auto request = jsonRequest(R"({"device_id":5})");
  drogon::HttpResponsePtr response;
  controller.deleteTelemetry(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k500InternalServerError), statusCode(response));
  EXPECT_EQ(1, repository->deletes);
}

TEST_CASE(ingest_controller_rejects_invalid_payload_without_calling_iot_core) {
  auto client = std::make_shared<test_support::IoTClientFixture>();
  IngestController controller(false, "test-key", client->shared());
  auto request = jsonRequest(R"({"device_id":0,"temperature":20})");

  drogon::HttpResponsePtr response;
  controller.ingest(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, client->sendCalls);
}

TEST_CASE(ingest_controller_forwards_valid_payload_and_returns_success) {
  auto client = std::make_shared<test_support::IoTClientFixture>();
  IngestController controller(false, "test-key", client->shared());
  auto request = jsonRequest(R"({"device_id":23,"temperature":18.5})");

  drogon::HttpResponsePtr response;
  controller.ingest(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(1, client->sendCalls);
  EXPECT_EQ(23, client->deviceId);
  EXPECT_EQ(18.5, client->temperature);
  EXPECT_EQ(static_cast<int>(drogon::k200OK), statusCode(response));
}

TEST_CASE(ingest_controller_maps_iot_core_failure_to_bad_gateway) {
  auto client = std::make_shared<test_support::IoTClientFixture>();
  client->failure = "IoT Core unavailable";
  IngestController controller(false, "test-key", client->shared());
  auto request = jsonRequest(R"({"device_id":23,"temperature":18.5})");

  drogon::HttpResponsePtr response;
  controller.ingest(request, [&](const auto &value) { response = value; });

  EXPECT_EQ(1, client->sendCalls);
  EXPECT_EQ(static_cast<int>(drogon::k502BadGateway), statusCode(response));
}

TEST_CASE(ingest_controller_rejects_malformed_json) {
  auto client = std::make_shared<test_support::IoTClientFixture>();
  IngestController controller(false, "test-key", client->shared());
  auto request = jsonRequest("{bad-json");
  drogon::HttpResponsePtr response;
  controller.ingest(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, client->sendCalls);
}

TEST_CASE(ingest_controller_rejects_bad_envelope_when_codec_is_enabled) {
  auto client = std::make_shared<test_support::IoTClientFixture>();
  IngestController controller(true, "test-key", client->shared());
  auto request = jsonRequest(R"({"checksum":"bad","ciphertext":"%%%"})");
  drogon::HttpResponsePtr response;
  controller.ingest(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k400BadRequest), statusCode(response));
  EXPECT_EQ(0, client->sendCalls);
}

TEST_CASE(ingest_controller_get_forwards_device_id_to_iot_client) {
  auto client = std::make_shared<test_support::IoTClientFixture>();
  IngestController controller(false, "test-key", client->shared());
  auto request = jsonRequest(R"({"device_id":23})");
  drogon::HttpResponsePtr response;
  controller.getTelemetry(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k200OK), statusCode(response));
  EXPECT_EQ(1, client->readCalls);
  EXPECT_EQ(23, client->deviceId);
}

TEST_CASE(ingest_controller_delete_forwards_device_id_to_iot_client) {
  auto client = std::make_shared<test_support::IoTClientFixture>();
  IngestController controller(false, "test-key", client->shared());
  auto request = jsonRequest(R"({"device_id":23})");
  drogon::HttpResponsePtr response;
  controller.deleteTelemetry(request, [&](const auto &value) { response = value; });
  EXPECT_EQ(static_cast<int>(drogon::k200OK), statusCode(response));
  EXPECT_EQ(1, client->deleteCalls);
  EXPECT_EQ(23, client->deviceId);
}
