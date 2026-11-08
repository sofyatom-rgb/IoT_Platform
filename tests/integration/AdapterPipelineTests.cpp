#include "TestFramework.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "common/http/HttpTransport.h"
#include "ingest/clients/IoTClient.h"
#include "ingest/services/AdapterService.h"

namespace {
class RecordingHttpTransport final : public iot::http::IHttpTransport {
 public:
  mutable std::vector<iot::http::Request> requests;
  iot::http::Result result{iot::http::Result::Ok};
  std::optional<iot::http::Response> response{
      iot::http::Response{200, R"({"notification_status":"not_triggered"})",
                          true,
                          {{"notification_status",
                            std::string("not_triggered")}},
                          {}}};

  void send(const iot::http::Request &request,
            iot::http::Completion completion) const override {
    requests.push_back(request);
    if (completion) completion(result, response);
  }
};

const iot::http::Request &lastRequest(const RecordingHttpTransport &transport) {
  return transport.requests.back();
}
}  // namespace

TEST_CASE(adapter_pipeline_uses_real_iot_client_and_posts_telemetry) {
  auto transport = std::make_shared<RecordingHttpTransport>();
  auto client = std::make_shared<IoTClient>(transport);
  AdapterService adapter(client);

  bool succeeded = false;
  std::string status;
  std::string error;
  adapter.processTelemetry(
      64, 27.25,
      [&](const std::string &value) {
        succeeded = true;
        status = value;
      },
      [&](const std::string &message) { error = message; });

  EXPECT_TRUE(succeeded);
  EXPECT_EQ("not_triggered", status);
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(std::size_t{1}, transport->requests.size());
  EXPECT_EQ(iot::http::Method::Post, lastRequest(*transport).method);
  EXPECT_EQ(std::string("/telemetry"), lastRequest(*transport).path);
  EXPECT_EQ(std::int64_t(64),
            std::get<std::int64_t>(lastRequest(*transport).jsonBody.at("device_id")));
  EXPECT_EQ(27.25,
            std::get<double>(lastRequest(*transport).jsonBody.at("temperature")));
}

TEST_CASE(adapter_pipeline_propagates_iot_core_http_error) {
  auto transport = std::make_shared<RecordingHttpTransport>();
  transport->response = iot::http::Response{503, "unavailable", false, {}, {}};
  AdapterService adapter(std::make_shared<IoTClient>(transport));

  int successCalls = 0;
  std::string error;
  adapter.processTelemetry(64, 27.25,
                           [&](const std::string &) { ++successCalls; },
                           [&](const std::string &message) { error = message; });

  EXPECT_EQ(0, successCalls);
  EXPECT_EQ(std::string("Failed to send to iot_core. HTTP Status: 503"), error);
}
