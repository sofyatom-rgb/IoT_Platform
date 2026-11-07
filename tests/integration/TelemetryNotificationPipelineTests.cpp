#include "TestFramework.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "common/http/HttpTransport.h"
#include "devices/services/IDevicesService.h"
#include "telemetry/clients/NotificationServiceClient.h"
#include "telemetry/repositories/ITelemetryRepository.h"
#include "telemetry/services/TelemetryService.h"
#include "users/services/IUsersService.h"

namespace {
class RecordingHttpTransport final : public iot::http::IHttpTransport {
 public:
  mutable std::vector<iot::http::Request> requests;
  void send(const iot::http::Request &request,
            iot::http::Completion completion) const override {
    requests.push_back(request);
    if (completion) {
      completion(iot::http::Result::Ok,
                 iot::http::Response{200, R"({"ok":true})", true,
                                     {{"ok", true}}, {}});
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

class DevicesServiceFake final : public IDevicesService {
 public:
  void save(int, const std::string &, const std::string &,
            std::function<void(int)> success,
            std::function<void(const std::string &)>) const override {
    success(1);
  }
  void deleteById(int, std::function<void()> success,
                  std::function<void(const std::string &)>) const override {
    success();
  }
  void getNotificationInfo(int deviceId,
                           std::function<void(int, const std::string &)> success,
                           std::function<void(const std::string &)>) const override {
    success(deviceId + 100, "Greenhouse sensor");
  }
  void updateById(int, int, const std::string &, const std::string &,
                  std::function<void()> success,
                  std::function<void(const std::string &)>) const override {
    success();
  }
  void getById(int, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)>) const override {
    success("{}");
  }
};

class UsersServiceFake final : public IUsersService {
 public:
  void save(const std::string &, const std::string &, const std::string &,
            const std::string &, std::function<void(int)> success,
            std::function<void(const std::string &)>) const override {
    success(1);
  }
  void deleteById(int, std::function<void()> success,
                  std::function<void(const std::string &)>) const override {
    success();
  }
  void getTelegramChatId(int, std::function<void(const std::string &)> success,
                         std::function<void(const std::string &)>) const override {
    success("chat-9001");
  }
  void updateById(int, const std::string &, const std::string &,
                  const std::string &, const std::string &,
                  std::function<void()> success,
                  std::function<void(const std::string &)>) const override {
    success();
  }
  void getById(int, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)>) const override {
    success("{}");
  }
};
}  // namespace

TEST_CASE(telemetry_pipeline_saves_then_sends_real_notification_http_request) {
  auto repository = std::make_shared<TelemetryRepositoryFake>();
  auto devices = std::make_shared<DevicesServiceFake>();
  auto users = std::make_shared<UsersServiceFake>();
  auto transport = std::make_shared<RecordingHttpTransport>();
  auto notificationClient = std::make_shared<NotificationServiceClient>(
      "http://notification-service", transport);
  TelemetryService service(repository, 30.0, devices, users, notificationClient);

  std::string notificationStatus;
  service.save(12, 41.75,
               [&](const std::string &status) { notificationStatus = status; },
               [](const std::string &) { EXPECT_TRUE(false); });

  EXPECT_EQ(std::string("sent"), notificationStatus);
  EXPECT_EQ(12, repository->savedDeviceId);
  EXPECT_EQ(41.75, repository->savedTemperature);
  EXPECT_EQ(std::size_t{1}, transport->requests.size());
  const auto &request = transport->requests.back();
  EXPECT_EQ(iot::http::Method::Post, request.method);
  EXPECT_EQ(std::string("/notifications/temperature"), request.path);
  EXPECT_EQ(std::string("chat-9001"),
            std::get<std::string>(request.jsonBody.at("chat_id")));
  EXPECT_EQ(std::string("Greenhouse sensor"),
            std::get<std::string>(request.jsonBody.at("device_name")));
  EXPECT_EQ(41.75, std::get<double>(request.jsonBody.at("temperature")));
  EXPECT_EQ(30.0, std::get<double>(request.jsonBody.at("threshold")));
}

TEST_CASE(telemetry_pipeline_clears_alert_state_when_temperature_is_safe) {
  auto repository = std::make_shared<TelemetryRepositoryFake>();
  auto transport = std::make_shared<RecordingHttpTransport>();
  auto notificationClient = std::make_shared<NotificationServiceClient>(
      "http://notification-service", transport);
  TelemetryService service(repository, 30.0,
                           std::make_shared<DevicesServiceFake>(),
                           std::make_shared<UsersServiceFake>(), notificationClient);

  std::string notificationStatus;
  service.save(12, 29.9,
               [&](const std::string &status) { notificationStatus = status; },
               [](const std::string &) { EXPECT_TRUE(false); });

  EXPECT_EQ(std::string("not_required"), notificationStatus);
  EXPECT_EQ(std::size_t{1}, transport->requests.size());
  const auto &request = transport->requests.back();
  EXPECT_EQ(iot::http::Method::Post, request.method);
  EXPECT_EQ(std::string("/notifications/temperature/normal"), request.path);
  EXPECT_EQ(std::int64_t(12),
            std::get<std::int64_t>(request.jsonBody.at("device_id")));
}
