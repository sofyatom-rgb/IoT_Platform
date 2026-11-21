#include "TestFramework.h"
#include "TestSupport.h"

#include <fakeit.hpp>
#include "devices/services/DevicesService.h"

#include <memory>
#include <string>

/// Проверяет создание устройства и получение данных для уведомления.
TEST_CASE(devices_service_forwards_creation_and_notification_info) {
  auto repositoryMock = std::make_shared<fakeit::Mock<IDevicesRepository>>();
  auto repository = test::sharedMock(repositoryMock);
  int savedOwner = -1;
  std::string savedName, savedType;
  int requestedDevice = -1;

  fakeit::When(Method(*repositoryMock, save))
      .Do([&](int owner, const std::string &name, const std::string &type,
              std::function<void(int)> success,
              std::function<void(const std::string &)>) {
        savedOwner = owner;
        savedName = name;
        savedType = type;
        success(52);
      });
  fakeit::When(Method(*repositoryMock, getNotificationInfo))
      .Do([&](int id, std::function<void(int, const std::string &)> success,
              std::function<void(const std::string &)>) {
        requestedDevice = id;
        success(9, "Kitchen sensor");
      });

  DevicesService service(repository);
  int id = -1;
  service.save(9, "Kitchen sensor", "thermometer",
               [&](int value) { id = value; },
               [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ(52, id);
  EXPECT_EQ(9, savedOwner);
  EXPECT_EQ("Kitchen sensor", savedName);
  EXPECT_EQ("thermometer", savedType);

  int owner = -1;
  std::string name;
  service.getNotificationInfo(52,
      [&](int ownerId, const std::string &deviceName) {
        owner = ownerId;
        name = deviceName;
      }, [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ(52, requestedDevice);
  EXPECT_EQ(9, owner);
  EXPECT_EQ("Kitchen sensor", name);
  fakeit::Verify(Method(*repositoryMock, save)).Once();
  fakeit::Verify(Method(*repositoryMock, getNotificationInfo)).Once();
}

/// Проверяет обновление устройства и передачу ошибки удаления.
TEST_CASE(devices_service_forwards_update_and_database_error) {
  auto repositoryMock = std::make_shared<fakeit::Mock<IDevicesRepository>>();
  auto repository = test::sharedMock(repositoryMock);
  int requestedId = -1;
  int ownerId = -1;
  std::string name, type;
  fakeit::When(Method(*repositoryMock, updateById))
      .Do([&](int id, int owner, const std::string &deviceName,
              const std::string &deviceType, std::function<void()> success,
              std::function<void(const std::string &)>) {
        requestedId = id;
        ownerId = owner;
        name = deviceName;
        type = deviceType;
        success();
      });
  fakeit::When(Method(*repositoryMock, deleteById))
      .Do([&](int id, std::function<void()>,
              std::function<void(const std::string &)> error) {
        requestedId = id;
        error("write failed");
      });

  DevicesService service(repository);
  bool updated = false;
  service.updateById(4, 7, "Office", "sensor", [&] { updated = true; },
                     [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(updated);
  EXPECT_EQ(4, requestedId);
  EXPECT_EQ(7, ownerId);
  EXPECT_EQ("Office", name);
  EXPECT_EQ("sensor", type);

  std::string observedError;
  service.deleteById(4, [] { EXPECT_TRUE(false); },
                     [&](const std::string &error) { observedError = error; });
  EXPECT_EQ("write failed", observedError);
  fakeit::Verify(Method(*repositoryMock, updateById)).Once();
  fakeit::Verify(Method(*repositoryMock, deleteById)).Once();
}

/// Проверяет получение JSON устройства из репозитория.
TEST_CASE(devices_service_forwards_device_lookup_result) {
  auto repositoryMock = std::make_shared<fakeit::Mock<IDevicesRepository>>();
  auto repository = test::sharedMock(repositoryMock);
  int requestedId = -1;
  fakeit::When(Method(*repositoryMock, getById))
      .Do([&](int id, std::function<void(const std::string &)> success,
              std::function<void(const std::string &)>) {
        requestedId = id;
        success("device-json");
      });

  DevicesService service(repository);
  std::string response;
  service.getById(63, [&](const std::string &value) { response = value; },
                  [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ(63, requestedId);
  EXPECT_EQ("device-json", response);
  fakeit::Verify(Method(*repositoryMock, getById)).Once();
}
