#include "TestFramework.h"
#include "HttpTransportMock.h"

#include <fakeit.hpp>

#include <memory>
#include <string>
#include <variant>

#include "ingest/clients/IoTClient.h"
#include "ingest/services/AdapterService.h"

/// Проверяет цепочку AdapterService → IoTClient → HTTP-транспорт.
TEST_CASE(adapter_pipeline_uses_real_iot_client_and_posts_telemetry) {
  test::HttpTransportMock transport;
  transport.response() = iot::http::Response{201, "{}", true, {}, {}};
  auto client = std::make_shared<IoTClient>(transport.shared());
  AdapterService adapter(client);

  bool succeeded = false;
  std::string error;
  adapter.processTelemetry(
      64, 27.25, [&] { succeeded = true; },
      [&](const std::string &message) { error = message; });

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(1U, transport.requests().size());
  EXPECT_EQ(iot::http::Method::Post, transport.lastRequest().method);
  EXPECT_EQ(std::string("/devices/64/telemetry"), transport.lastRequest().path);
  EXPECT_TRUE(transport.lastRequest().jsonBody.find("device_id") ==
              transport.lastRequest().jsonBody.end());
  EXPECT_EQ(27.25, std::get<double>(
      transport.lastRequest().jsonBody.at("temperature")));
  fakeit::Verify(Method(transport.mock(), send)).Once();
}

/// Проверяет передачу HTTP-ошибки IoT Core обратно в AdapterService.
TEST_CASE(adapter_pipeline_propagates_iot_core_http_error) {
  test::HttpTransportMock transport;
  transport.response() = iot::http::Response{503, "unavailable", false, {}, {}};
  AdapterService adapter(std::make_shared<IoTClient>(transport.shared()));

  int successCalls = 0;
  std::string error;
  adapter.processTelemetry(64, 27.25, [&] { ++successCalls; },
                           [&](const std::string &message) { error = message; });

  EXPECT_EQ(0, successCalls);
  EXPECT_EQ(std::string("Failed to send to iot_core. HTTP Status: 503"), error);
  fakeit::Verify(Method(transport.mock(), send)).Once();
}
