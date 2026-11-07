#include "TestFramework.h"

#include "ingest/services/AdapterService.h"

#include <memory>

namespace {
class FakeIoTClient final : public IIoTClient {
 public:
  mutable int deviceId{-1};
  mutable double temperature{0.0};
  mutable std::string failure;
  mutable int sends{0}, reads{0}, deletes{0};

  void sendTelemetry(int id, double value, std::function<void()> success,
                     std::function<void(const std::string &)> error) const override {
    deviceId = id; temperature = value; ++sends;
    if (failure.empty()) success(); else error(failure);
  }
  void getTelemetry(int id, std::function<void(const std::string &)> success,
                    std::function<void(const std::string &)> error) const override {
    deviceId = id; ++reads;
    if (failure.empty()) success("telemetry-json"); else error(failure);
  }
  void deleteTelemetry(int id, std::function<void()> success,
                       std::function<void(const std::string &)> error) const override {
    deviceId = id; ++deletes;
    if (failure.empty()) success(); else error(failure);
  }
};
}  // namespace

TEST_CASE(adapter_service_delegates_ingest_and_preserves_callbacks) {
  auto client = std::make_shared<FakeIoTClient>();
  AdapterService service(client);
  bool succeeded = false;
  service.processTelemetry(11, 23.5, [&] { succeeded = true; },
                           [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(11, client->deviceId);
  EXPECT_EQ(23.5, client->temperature);
  EXPECT_EQ(1, client->sends);
}

TEST_CASE(adapter_service_delegates_read_delete_and_error) {
  auto client = std::make_shared<FakeIoTClient>();
  AdapterService service(client);
  std::string body;
  service.getTelemetry(12, [&](const std::string &value) { body = value; },
                       [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("telemetry-json", body);
  EXPECT_EQ(1, client->reads);

  client->failure = "core unavailable";
  std::string error;
  service.deleteTelemetry(12, [] { EXPECT_TRUE(false); },
                          [&](const std::string &value) { error = value; });
  EXPECT_EQ("core unavailable", error);
  EXPECT_EQ(1, client->deletes);
}
