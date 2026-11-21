#pragma once

#include "TestSupport.h"

#include <fakeit.hpp>

#include "devices/services/IDevicesService.h"
#include "ingest/clients/IIoTClient.h"
#include "telemetry/clients/INotificationServiceClient.h"
#include "telemetry/repositories/ITelemetryRepository.h"
#include "telemetry/services/TelemetryService.h"
#include "users/services/IUsersService.h"
#include "clients/ITelegramBotClient.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace test_support {

/// Настраивает FakeIt-мок пользовательского сервиса и хранит параметры вызовов.
struct UsersServiceFixture {
  std::shared_ptr<fakeit::Mock<IUsersService>> mock =
      std::make_shared<fakeit::Mock<IUsersService>>();
  int saveCalls{0};
  int deleteCalls{0};
  int updateCalls{0};
  int getCalls{0};
  int savedId{101};
  int requestedId{-1};
  std::string email;
  std::string password;
  std::string name;
  std::string chatId;
  std::string failure;
  std::string getResponse{R"({"id":42,"email":"test@example.com"})"};

  UsersServiceFixture() {
    fakeit::When(Method(*mock, save)).AlwaysDo(
        [this](const std::string &emailValue, const std::string &passwordValue,
               const std::string &nameValue, const std::string &chatIdValue,
               std::function<void(int)> success,
               std::function<void(const std::string &)> error) {
          ++saveCalls;
          email = emailValue;
          password = passwordValue;
          name = nameValue;
          chatId = chatIdValue;
          if (failure.empty()) success(savedId); else error(failure);
        });
    fakeit::When(Method(*mock, deleteById)).AlwaysDo(
        [this](int id, std::function<void()> success,
               std::function<void(const std::string &)> error) {
          ++deleteCalls;
          requestedId = id;
          if (failure.empty()) success(); else error(failure);
        });
    fakeit::When(Method(*mock, getTelegramChatId)).AlwaysDo(
        [this](int id, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) {
          requestedId = id;
          if (failure.empty()) success(chatId); else error(failure);
        });
    fakeit::When(Method(*mock, updateById)).AlwaysDo(
        [this](int id, const std::string &emailValue,
               const std::string &passwordValue, const std::string &nameValue,
               const std::string &chatIdValue, std::function<void()> success,
               std::function<void(const std::string &)> error) {
          ++updateCalls;
          requestedId = id;
          email = emailValue;
          password = passwordValue;
          name = nameValue;
          chatId = chatIdValue;
          if (failure.empty()) success(); else error(failure);
        });
    fakeit::When(Method(*mock, getById)).AlwaysDo(
        [this](int id, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) {
          ++getCalls;
          requestedId = id;
          if (failure.empty()) success(getResponse); else error(failure);
        });
  }

  /// Возвращает мок через контракт сервиса.
  std::shared_ptr<IUsersService> shared() const { return test::sharedMock(mock); }
};

/// Настраивает FakeIt-мок сервиса устройств и записывает аргументы вызовов.
struct DevicesServiceFixture {
  std::shared_ptr<fakeit::Mock<IDevicesService>> mock =
      std::make_shared<fakeit::Mock<IDevicesService>>();
  int saveCalls{0};
  int deleteCalls{0};
  int updateCalls{0};
  int getCalls{0};
  int savedId{202};
  int requestedId{-1};
  int ownerId{-1};
  std::string name;
  std::string type;
  std::string failure;
  std::string getResponse{R"({"id":202,"name":"Boiler"})"};

  DevicesServiceFixture() {
    fakeit::When(Method(*mock, save)).AlwaysDo(
        [this](int owner, const std::string &deviceName,
               const std::string &deviceType, std::function<void(int)> success,
               std::function<void(const std::string &)> error) {
          ++saveCalls;
          ownerId = owner;
          name = deviceName;
          type = deviceType;
          if (failure.empty()) success(savedId); else error(failure);
        });
    fakeit::When(Method(*mock, deleteById)).AlwaysDo(
        [this](int id, std::function<void()> success,
               std::function<void(const std::string &)> error) {
          ++deleteCalls;
          requestedId = id;
          if (failure.empty()) success(); else error(failure);
        });
    fakeit::When(Method(*mock, getNotificationInfo)).AlwaysDo(
        [this](int id,
               std::function<void(int, const std::string &)> success,
               std::function<void(const std::string &)> error) {
          requestedId = id;
          if (failure.empty()) success(ownerId, name); else error(failure);
        });
    fakeit::When(Method(*mock, updateById)).AlwaysDo(
        [this](int id, int owner, const std::string &deviceName,
               const std::string &deviceType, std::function<void()> success,
               std::function<void(const std::string &)> error) {
          ++updateCalls;
          requestedId = id;
          ownerId = owner;
          name = deviceName;
          type = deviceType;
          if (failure.empty()) success(); else error(failure);
        });
    fakeit::When(Method(*mock, getById)).AlwaysDo(
        [this](int id, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) {
          ++getCalls;
          requestedId = id;
          if (failure.empty()) success(getResponse); else error(failure);
        });
  }

  /// Возвращает мок через контракт сервиса.
  std::shared_ptr<IDevicesService> shared() const { return test::sharedMock(mock); }
};

/// Настраивает репозиторий телеметрии для тестирования HTTP-контроллера.
struct TelemetryRepositoryFixture {
  std::shared_ptr<fakeit::Mock<ITelemetryRepository>> mock =
      std::make_shared<fakeit::Mock<ITelemetryRepository>>();
  int saves{0};
  int reads{0};
  int deletes{0};
  int deviceId{-1};
  double temperature{0.0};
  std::string failure;

  TelemetryRepositoryFixture() {
    fakeit::When(Method(*mock, save)).AlwaysDo(
        [this](int id, double value, std::function<void()> success,
               std::function<void(const std::string &)> error) {
          ++saves;
          deviceId = id;
          temperature = value;
          if (failure.empty()) success(); else error(failure);
        });
    fakeit::When(Method(*mock, getByDeviceId)).AlwaysDo(
        [this](int id, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) {
          ++reads;
          deviceId = id;
          if (failure.empty()) success(R"([{"device_id":5,"temperature":20.5}])");
          else error(failure);
        });
    fakeit::When(Method(*mock, deleteByDeviceId)).AlwaysDo(
        [this](int id, std::function<void()> success,
               std::function<void(const std::string &)> error) {
          ++deletes;
          deviceId = id;
          if (failure.empty()) success(); else error(failure);
        });
  }

  /// Возвращает мок через контракт репозитория.
  std::shared_ptr<ITelemetryRepository> shared() const {
    return test::sharedMock(mock);
  }
};

/// Создаёт настоящий TelemetryService с FakeIt-моками внешних компонентов.
inline std::shared_ptr<TelemetryService> makeTelemetryService(
    const std::shared_ptr<TelemetryRepositoryFixture> &repository) {
  auto devicesMock = std::make_shared<fakeit::Mock<IDevicesService>>();
  auto usersMock = std::make_shared<fakeit::Mock<IUsersService>>();
  auto notificationMock =
      std::make_shared<fakeit::Mock<INotificationServiceClient>>();

  fakeit::When(Method(*devicesMock, getNotificationInfo)).AlwaysDo(
      [](int, std::function<void(int, const std::string &)> success,
         std::function<void(const std::string &)>) { success(42, "Boiler"); });
  fakeit::When(Method(*usersMock, getTelegramChatId)).AlwaysDo(
      [](int, std::function<void(const std::string &)> success,
         std::function<void(const std::string &)>) { success("chat-42"); });
  fakeit::When(Method(*notificationMock, sendTemperatureAlert)).AlwaysDo(
      [](const std::string &, const std::string &, double, double,
         std::function<void()> success,
         std::function<void(const std::string &)>) { success(); });

  return std::make_shared<TelemetryService>(
      repository->shared(), test::sharedMock(devicesMock),
      test::sharedMock(usersMock), test::sharedMock(notificationMock), 30.0);
}

/// Настраивает Telegram-клиент и сохраняет отправленные параметры.
struct TelegramClientFixture {
  std::shared_ptr<fakeit::Mock<ITelegramBotClient>> mock =
      std::make_shared<fakeit::Mock<ITelegramBotClient>>();
  int calls{0};
  std::string chatId;
  std::string message;
  std::string failure;

  TelegramClientFixture() {
    fakeit::When(Method(*mock, sendMessage)).AlwaysDo(
        [this](const std::string &id, const std::string &text,
               std::function<void()> success,
               std::function<void(const std::string &)> error) {
          ++calls;
          chatId = id;
          message = text;
          if (failure.empty()) success(); else error(failure);
        });
  }

  /// Возвращает мок через контракт Telegram-клиента.
  std::shared_ptr<ITelegramBotClient> shared() const { return test::sharedMock(mock); }
};

/// Настраивает IoT-клиент и записывает запросы контроллера.
struct IoTClientFixture {
  std::shared_ptr<fakeit::Mock<IIoTClient>> mock =
      std::make_shared<fakeit::Mock<IIoTClient>>();
  int sendCalls{0};
  int readCalls{0};
  int deleteCalls{0};
  int deviceId{-1};
  double temperature{0.0};
  std::string failure;

  IoTClientFixture() {
    fakeit::When(Method(*mock, sendTelemetry)).AlwaysDo(
        [this](int id, double value, std::function<void()> success,
               std::function<void(const std::string &)> error) {
          ++sendCalls;
          deviceId = id;
          temperature = value;
          if (failure.empty()) success(); else error(failure);
        });
    fakeit::When(Method(*mock, getTelemetry)).AlwaysDo(
        [this](int id, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) {
          ++readCalls;
          deviceId = id;
          if (failure.empty()) success("[]"); else error(failure);
        });
    fakeit::When(Method(*mock, deleteTelemetry)).AlwaysDo(
        [this](int id, std::function<void()> success,
               std::function<void(const std::string &)> error) {
          ++deleteCalls;
          deviceId = id;
          if (failure.empty()) success(); else error(failure);
        });
  }

  /// Возвращает мок через контракт IoT-клиента.
  std::shared_ptr<IIoTClient> shared() const { return test::sharedMock(mock); }
};

}  // namespace test_support
