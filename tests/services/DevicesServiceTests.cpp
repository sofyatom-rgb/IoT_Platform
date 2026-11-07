#include "TestFramework.h"

#include "devices/services/DevicesService.h"

#include <memory>

namespace {
class FakeDevicesRepository final : public IDevicesRepository {
 public:
  mutable int deviceId{-1};
  mutable int ownerId{-1};
  mutable std::string name, type, failure;

  void save(int owner, const std::string &deviceName, const std::string &deviceType,
            std::function<void(int)> success,
            std::function<void(const std::string &)> error) const override {
    ownerId = owner; name = deviceName; type = deviceType;
    if (failure.empty()) success(52); else error(failure);
  }
  void deleteById(int id, std::function<void()> success,
                  std::function<void(const std::string &)> error) const override {
    deviceId = id;
    if (failure.empty()) success(); else error(failure);
  }
  void getNotificationInfo(int id,
                           std::function<void(int, const std::string &)> success,
                           std::function<void(const std::string &)> error) const override {
    deviceId = id;
    if (failure.empty()) success(9, "Kitchen sensor"); else error(failure);
  }
  void updateById(int id, int owner, const std::string &deviceName,
                  const std::string &deviceType, std::function<void()> success,
                  std::function<void(const std::string &)> error) const override {
    deviceId = id; ownerId = owner; name = deviceName; type = deviceType;
    if (failure.empty()) success(); else error(failure);
  }
  void getById(int id, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) const override {
    deviceId = id;
    if (failure.empty()) success("device-json"); else error(failure);
  }
};
}  // namespace

TEST_CASE(devices_service_forwards_creation_and_notification_info) {
  auto repository = std::make_shared<FakeDevicesRepository>();
  DevicesService service(repository);
  int id = -1;
  service.save(9, "Kitchen sensor", "thermometer",
               [&](int value) { id = value; },
               [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ(52, id);
  EXPECT_EQ(9, repository->ownerId);
  EXPECT_EQ("Kitchen sensor", repository->name);
  EXPECT_EQ("thermometer", repository->type);

  int owner = -1;
  std::string name;
  service.getNotificationInfo(52,
      [&](int ownerId, const std::string &deviceName) { owner = ownerId; name = deviceName; },
      [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ(52, repository->deviceId);
  EXPECT_EQ(9, owner);
  EXPECT_EQ("Kitchen sensor", name);
}

TEST_CASE(devices_service_forwards_update_and_database_error) {
  auto repository = std::make_shared<FakeDevicesRepository>();
  DevicesService service(repository);
  bool updated = false;
  service.updateById(4, 7, "Office", "sensor", [&] { updated = true; },
                     [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(updated);
  EXPECT_EQ(4, repository->deviceId);
  EXPECT_EQ(7, repository->ownerId);

  repository->failure = "write failed";
  std::string observedError;
  service.deleteById(4, [] { EXPECT_TRUE(false); },
                     [&](const std::string &error) { observedError = error; });
  EXPECT_EQ("write failed", observedError);
}

TEST_CASE(devices_service_forwards_device_lookup_result) {
  auto repository = std::make_shared<FakeDevicesRepository>();
  DevicesService service(repository);
  std::string response;
  service.getById(63, [&](const std::string &value) { response = value; },
                  [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ(63, repository->deviceId);
  EXPECT_EQ("device-json", response);
}
