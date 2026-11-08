#include "TestFramework.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "common/http/HttpTransport.h"
#include "telemetry/clients/RuleEngineClient.h"
#include "telemetry/repositories/ITelemetryRepository.h"
#include "telemetry/services/TelemetryService.h"

namespace {
class RecordingHttpTransport final : public iot::http::IHttpTransport {
 public:
  mutable std::vector<iot::http::Request> requests;
  void send(const iot::http::Request &request,
            iot::http::Completion completion) const override {
    requests.push_back(request);
    if (completion) {
      completion(iot::http::Result::Ok,
                 iot::http::Response{
                     200,
                     R"({"status":"ok","notification_status":"telegram=sent; email=sent"})",
                     true,
                     {{"status", std::string("ok")},
                      {"notification_status",
                       std::string("telegram=sent; email=sent")}},
                     {}});
    }
  }
};

class TelemetryRepositoryFake final : public ITelemetryRepository {
 public:
  mutable int savedDeviceId{-1};
  mutable double savedTemperature{0.0};
  void save(int deviceId, double temperature, std::function<void()> success,
            std::function<void(const std::string &)>) const override {
    savedDeviceId = deviceId;
    savedTemperature = temperature;
    success();
  }
  void getByDeviceId(int, std::function<void(const std::string &)> success,
                     std::function<void(const std::string &)>) const override {
    success("[]");
  }
  void deleteByDeviceId(int, std::function<void()> success,
                        std::function<void(const std::string &)>) const override {
    success();
  }
};
}  // namespace

TEST_CASE(telemetry_pipeline_persists_then_posts_reading_to_rule_engine) {
  auto repository = std::make_shared<TelemetryRepositoryFake>();
  auto transport = std::make_shared<RecordingHttpTransport>();
  auto ruleEngineClient = std::make_shared<RuleEngineClient>(
      "http://rule-engine", transport);
  TelemetryService service(repository, ruleEngineClient);

  std::string notificationStatus;
  service.save(12, 41.75,
               [&](const std::string &status) { notificationStatus = status; },
               [](const std::string &) { EXPECT_TRUE(false); });

  EXPECT_EQ(std::string("telegram=sent; email=sent"), notificationStatus);
  EXPECT_EQ(12, repository->savedDeviceId);
  EXPECT_EQ(41.75, repository->savedTemperature);
  EXPECT_EQ(std::size_t{1}, transport->requests.size());
  const auto &request = transport->requests.back();
  EXPECT_EQ(iot::http::Method::Post, request.method);
  EXPECT_EQ(std::string("/telemetry"), request.path);
  EXPECT_EQ(std::int64_t(12),
            std::get<std::int64_t>(request.jsonBody.at("device_id")));
  EXPECT_EQ(41.75,
            std::get<double>(request.jsonBody.at("temperature")));
}
