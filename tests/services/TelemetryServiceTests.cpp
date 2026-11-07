#include "TestFramework.h"

#include "telemetry/services/TelemetryService.h"

#include <memory>

namespace {
class FakeTelemetryRepository final : public ITelemetryRepository {
 public:
  mutable int deviceId{-1};
  mutable double temperature{0.0};
  mutable std::string failure;
  mutable int saves{0}, reads{0}, deletes{0};

  void save(int id, double value, std::function<void()> success,
            std::function<void(const std::string &)> error) const override {
    deviceId = id; temperature = value; ++saves;
    if (failure.empty()) success(); else error(failure);
  }
  void getByDeviceId(int id, std::function<void(const std::string &)> success,
                     std::function<void(const std::string &)> error) const override {
    deviceId = id; ++reads;
    if (failure.empty()) success("[telemetry]"); else error(failure);
  }
  void deleteByDeviceId(int id, std::function<void()> success,
                        std::function<void(const std::string &)> error) const override {
    deviceId = id; ++deletes;
    if (failure.empty()) success(); else error(failure);
  }
};

class FakeDevicesService final : public IDevicesService {
 public:
  mutable int requestedDeviceId{-1};
  mutable std::string deviceName{"Living room"}, failure;
  mutable int ownerId{42}, lookups{0};

  void save(int, const std::string &, const std::string &,
            std::function<void(int)> success,
            std::function<void(const std::string &)> error) const override {
    if (failure.empty()) success(1); else error(failure);
  }
  void deleteById(int, std::function<void()> success,
                  std::function<void(const std::string &)> error) const override {
    if (failure.empty()) success(); else error(failure);
  }
  void getNotificationInfo(int id,
                           std::function<void(int, const std::string &)> success,
                           std::function<void(const std::string &)> error) const override {
    requestedDeviceId = id; ++lookups;
    if (failure.empty()) success(ownerId, deviceName); else error(failure);
  }
  void updateById(int, int, const std::string &, const std::string &,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const override {
    if (failure.empty()) success(); else error(failure);
  }
  void getById(int, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) const override {
    if (failure.empty()) success("device"); else error(failure);
  }
};

class FakeUsersService final : public IUsersService {
 public:
  mutable int requestedUserId{-1};
  mutable std::string chatId{"chat-77"}, failure;
  mutable int lookups{0};

  void save(const std::string &, const std::string &, const std::string &,
            const std::string &, std::function<void(int)> success,
            std::function<void(const std::string &)> error) const override {
    if (failure.empty()) success(1); else error(failure);
  }
  void deleteById(int, std::function<void()> success,
                  std::function<void(const std::string &)> error) const override {
    if (failure.empty()) success(); else error(failure);
  }
  void getTelegramChatId(int id, std::function<void(const std::string &)> success,
                         std::function<void(const std::string &)> error) const override {
    requestedUserId = id; ++lookups;
    if (failure.empty()) success(chatId); else error(failure);
  }
  void updateById(int, const std::string &, const std::string &, const std::string &,
                  const std::string &, std::function<void()> success,
                  std::function<void(const std::string &)> error) const override {
    if (failure.empty()) success(); else error(failure);
  }
  void getById(int, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) const override {
    if (failure.empty()) success("user"); else error(failure);
  }
};

class FakeNotificationClient final : public INotificationServiceClient {
 public:
  mutable std::string chatId, deviceName, failure;
  mutable double temperature{0.0}, threshold{0.0};
  mutable int calls{0};

  void sendTemperatureAlert(const std::string &chat, const std::string &name,
                            double value, double limit,
                            std::function<void()> success,
                            std::function<void(const std::string &)> error) const override {
    chatId = chat; deviceName = name; temperature = value; threshold = limit; ++calls;
    if (failure.empty()) success(); else error(failure);
  }
};

struct Fixture {
  std::shared_ptr<FakeTelemetryRepository> repository =
      std::make_shared<FakeTelemetryRepository>();
  std::shared_ptr<FakeDevicesService> devices =
      std::make_shared<FakeDevicesService>();
  std::shared_ptr<FakeUsersService> users =
      std::make_shared<FakeUsersService>();
  std::shared_ptr<FakeNotificationClient> notifications =
      std::make_shared<FakeNotificationClient>();
  TelemetryService service{repository, 30.0, devices, users, notifications};
};
}  // namespace

TEST_CASE(telemetry_below_or_equal_threshold_does_not_lookup_or_notify) {
  Fixture f;
  std::string result;
  f.service.save(5, 30.0, [&](const std::string &value) { result = value; },
                 [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("not_required", result);
  EXPECT_EQ(5, f.repository->deviceId);
  EXPECT_EQ(30.0, f.repository->temperature);
  EXPECT_EQ(0, f.devices->lookups);
  EXPECT_EQ(0, f.users->lookups);
  EXPECT_EQ(0, f.notifications->calls);
}

TEST_CASE(telemetry_above_threshold_resolves_user_and_sends_notification) {
  Fixture f;
  std::string result;
  f.service.save(8, 31.25, [&](const std::string &value) { result = value; },
                 [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("sent", result);
  EXPECT_EQ(8, f.devices->requestedDeviceId);
  EXPECT_EQ(42, f.users->requestedUserId);
  EXPECT_EQ("chat-77", f.notifications->chatId);
  EXPECT_EQ("Living room", f.notifications->deviceName);
  EXPECT_EQ(31.25, f.notifications->temperature);
  EXPECT_EQ(30.0, f.notifications->threshold);
  EXPECT_EQ(1, f.notifications->calls);
}

TEST_CASE(telemetry_skips_notification_when_user_has_no_chat_id) {
  Fixture f;
  f.users->chatId.clear();
  std::string result;
  f.service.save(9, 35.0, [&](const std::string &value) { result = value; },
                 [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("no_chat_id", result);
  EXPECT_EQ(0, f.notifications->calls);
}

TEST_CASE(telemetry_reports_device_and_user_lookup_failures) {
  Fixture deviceFailure;
  deviceFailure.devices->failure = "device missing";
  std::string deviceResult;
  deviceFailure.service.save(4, 40.0,
      [&](const std::string &value) { deviceResult = value; },
      [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("device_lookup_failed: device missing", deviceResult);
  EXPECT_EQ(0, deviceFailure.users->lookups);

  Fixture userFailure;
  userFailure.users->failure = "user missing";
  std::string userResult;
  userFailure.service.save(4, 40.0,
      [&](const std::string &value) { userResult = value; },
      [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("user_lookup_failed: user missing", userResult);
  EXPECT_EQ(0, userFailure.notifications->calls);
}

TEST_CASE(telemetry_reports_notification_and_repository_failures) {
  Fixture notificationFailure;
  notificationFailure.notifications->failure = "HTTP 502";
  std::string notificationResult;
  notificationFailure.service.save(4, 40.0,
      [&](const std::string &value) { notificationResult = value; },
      [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("delivery_failed: HTTP 502", notificationResult);

  Fixture repositoryFailure;
  repositoryFailure.repository->failure = "database down";
  std::string error;
  std::string result;
  repositoryFailure.service.save(4, 40.0,
      [&](const std::string &value) { result = value; },
      [&](const std::string &value) { error = value; });
  EXPECT_EQ("database down", error);
  EXPECT_EQ("", result);
  EXPECT_EQ(0, repositoryFailure.devices->lookups);
}

TEST_CASE(telemetry_read_and_delete_are_forwarded_to_repository) {
  Fixture f;
  std::string body;
  f.service.getByDeviceId(14, [&](const std::string &value) { body = value; },
                          [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("[telemetry]", body);
  EXPECT_EQ(14, f.repository->deviceId);
  EXPECT_EQ(1, f.repository->reads);

  bool deleted = false;
  f.service.deleteByDeviceId(14, [&] { deleted = true; },
                             [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(deleted);
  EXPECT_EQ(1, f.repository->deletes);
}
