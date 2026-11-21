#include "TestFramework.h"
#include "TestSupport.h"

#include <fakeit.hpp>
#include "ingest/services/AdapterService.h"

#include <memory>
#include <string>

/// Проверяет передачу показания в IoT-клиент и возврат его callback.
TEST_CASE(adapter_service_delegates_ingest_and_preserves_callbacks) {
  auto clientMock = std::make_shared<fakeit::Mock<IIoTClient>>();
  auto client = test::sharedMock(clientMock);
  int receivedId = -1;
  double receivedTemperature = 0.0;
  fakeit::When(Method(*clientMock, sendTelemetry))
      .Do([&](int id, double temperature, std::function<void()> success,
              std::function<void(const std::string &)>) {
        receivedId = id;
        receivedTemperature = temperature;
        success();
      });

  AdapterService service(client);
  bool succeeded = false;
  service.processTelemetry(11, 23.5, [&] { succeeded = true; },
                           [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(11, receivedId);
  EXPECT_EQ(23.5, receivedTemperature);
  fakeit::Verify(Method(*clientMock, sendTelemetry)).Once();
}

/// Проверяет чтение и передачу ошибки IoT-клиента.
TEST_CASE(adapter_service_delegates_read_delete_and_error) {
  auto clientMock = std::make_shared<fakeit::Mock<IIoTClient>>();
  auto client = test::sharedMock(clientMock);
  fakeit::When(Method(*clientMock, getTelemetry))
      .Do([](int, std::function<void(const std::string &)> success,
             std::function<void(const std::string &)>) {
        success("telemetry-json");
      });
  fakeit::When(Method(*clientMock, deleteTelemetry))
      .Do([](int, std::function<void()>,
             std::function<void(const std::string &)> error) {
        error("core unavailable");
      });

  AdapterService service(client);
  std::string body;
  service.getTelemetry(12, [&](const std::string &value) { body = value; },
                       [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("telemetry-json", body);

  std::string error;
  service.deleteTelemetry(12, [] { EXPECT_TRUE(false); },
                          [&](const std::string &value) { error = value; });
  EXPECT_EQ("core unavailable", error);
  fakeit::Verify(Method(*clientMock, getTelemetry)).Once();
  fakeit::Verify(Method(*clientMock, deleteTelemetry)).Once();
}
