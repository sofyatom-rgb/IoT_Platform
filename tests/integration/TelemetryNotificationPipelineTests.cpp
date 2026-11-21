#include "TestFramework.h"
#include "HttpTransportMock.h"
#include "TestSupport.h"

#include <fakeit.hpp>

#include <functional>
#include <memory>
#include <string>
#include <variant>

#include "devices/services/IDevicesService.h"
#include "telemetry/clients/NotificationServiceClient.h"
#include "telemetry/repositories/ITelemetryRepository.h"
#include "telemetry/services/TelemetryService.h"
#include "users/services/IUsersService.h"

namespace {
/// Создаёт реальный TelemetryService с FakeIt-зависимостями и HTTP-моком.
struct PipelineFixture {
  std::shared_ptr<fakeit::Mock<ITelemetryRepository>> repository =
      std::make_shared<fakeit::Mock<ITelemetryRepository>>();
  std::shared_ptr<fakeit::Mock<IDevicesService>> devices =
      std::make_shared<fakeit::Mock<IDevicesService>>();
  std::shared_ptr<fakeit::Mock<IUsersService>> users =
      std::make_shared<fakeit::Mock<IUsersService>>();
  test::HttpTransportMock transport;

  int savedDeviceId{-1};
  double savedTemperature{0.0};
  int repositorySaveCalls{0};
  int deviceLookupCalls{0};
  int userLookupCalls{0};

  std::shared_ptr<NotificationServiceClient> notificationClient =
      std::make_shared<NotificationServiceClient>(
          "http://notification-service", transport.shared());
  TelemetryService service{test::sharedMock(repository),
                           test::sharedMock(devices),
                           test::sharedMock(users), notificationClient, 30.0};

  PipelineFixture() {
    fakeit::When(Method(*repository, save)).AlwaysDo(
        [this](int deviceId, double temperature, std::function<void()> success,
               std::function<void(const std::string &)>) {
          ++repositorySaveCalls;
          savedDeviceId = deviceId;
          savedTemperature = temperature;
          success();
        });
    fakeit::When(Method(*devices, getNotificationInfo)).AlwaysDo(
        [this](int deviceId,
               std::function<void(int, const std::string &)> success,
               std::function<void(const std::string &)>) {
          ++deviceLookupCalls;
          success(deviceId + 100, "Greenhouse sensor");
        });
    fakeit::When(Method(*users, getTelegramChatId)).AlwaysDo(
        [this](int,
               std::function<void(const std::string &)> success,
               std::function<void(const std::string &)>) {
          ++userLookupCalls;
          success("chat-9001");
        });
    transport.response() = iot::http::Response{
        200, R"({"ok":true})", true, {{"ok", true}}, {}};
  }
};
}  // namespace

/// Проверяет сохранение показания и отправку настоящим HTTP-клиентом уведомления.
TEST_CASE(telemetry_pipeline_saves_then_sends_real_notification_http_request) {
  PipelineFixture fixture;
  std::string notificationStatus;
  fixture.service.save(
      12, 41.75,
      [&](const std::string &status) { notificationStatus = status; },
      [](const std::string &) { EXPECT_TRUE(false); });

  EXPECT_EQ(std::string("sent"), notificationStatus);
  EXPECT_EQ(12, fixture.savedDeviceId);
  EXPECT_EQ(41.75, fixture.savedTemperature);
  EXPECT_EQ(1, fixture.repositorySaveCalls);
  EXPECT_EQ(1, fixture.deviceLookupCalls);
  EXPECT_EQ(1, fixture.userLookupCalls);
  EXPECT_EQ(1U, fixture.transport.requests().size());
  const auto &request = fixture.transport.lastRequest();
  EXPECT_EQ(iot::http::Method::Post, request.method);
  EXPECT_EQ(std::string("/notifications/temperature-alerts"), request.path);
  EXPECT_EQ(std::string("chat-9001"),
            std::get<std::string>(request.jsonBody.at("chat_id")));
  EXPECT_EQ(std::string("Greenhouse sensor"),
            std::get<std::string>(request.jsonBody.at("device_name")));
  EXPECT_EQ(41.75, std::get<double>(request.jsonBody.at("temperature")));
  EXPECT_EQ(30.0, std::get<double>(request.jsonBody.at("threshold")));
  fakeit::Verify(Method(*fixture.repository, save)).Once();
  fakeit::Verify(Method(*fixture.devices, getNotificationInfo)).Once();
  fakeit::Verify(Method(*fixture.users, getTelegramChatId)).Once();
  fakeit::Verify(Method(fixture.transport.mock(), send)).Once();
}

/// Проверяет, что безопасная температура не запускает поиск адресата и HTTP-запрос.
TEST_CASE(telemetry_pipeline_does_not_send_http_when_temperature_is_safe) {
  PipelineFixture fixture;
  std::string notificationStatus;
  fixture.service.save(
      12, 29.9,
      [&](const std::string &status) { notificationStatus = status; },
      [](const std::string &) { EXPECT_TRUE(false); });

  EXPECT_EQ(std::string("not_required"), notificationStatus);
  EXPECT_EQ(1, fixture.repositorySaveCalls);
  EXPECT_EQ(0, fixture.deviceLookupCalls);
  EXPECT_EQ(0, fixture.userLookupCalls);
  EXPECT_TRUE(fixture.transport.requests().empty());
  fakeit::Verify(Method(*fixture.repository, save)).Once();
  fakeit::Verify(Method(*fixture.devices, getNotificationInfo)).Never();
  fakeit::Verify(Method(*fixture.users, getTelegramChatId)).Never();
  fakeit::Verify(Method(fixture.transport.mock(), send)).Never();
}
