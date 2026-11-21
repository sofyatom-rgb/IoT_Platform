#include "TestFramework.h"
#include "TestSupport.h"

#include <fakeit.hpp>
#include "telemetry/services/TelemetryService.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

/// Собирает TelemetryService с управляемыми FakeIt-зависимостями.
struct TelemetryFixture {
  std::shared_ptr<fakeit::Mock<ITelemetryRepository>> repositoryMock =
      std::make_shared<fakeit::Mock<ITelemetryRepository>>();
  std::shared_ptr<fakeit::Mock<IDevicesService>> devicesMock =
      std::make_shared<fakeit::Mock<IDevicesService>>();
  std::shared_ptr<fakeit::Mock<IUsersService>> usersMock =
      std::make_shared<fakeit::Mock<IUsersService>>();
  std::shared_ptr<fakeit::Mock<INotificationServiceClient>> notificationMock =
      std::make_shared<fakeit::Mock<INotificationServiceClient>>();

  std::shared_ptr<ITelemetryRepository> repository =
      test::sharedMock(repositoryMock);
  std::shared_ptr<IDevicesService> devices = test::sharedMock(devicesMock);
  std::shared_ptr<IUsersService> users = test::sharedMock(usersMock);
  std::shared_ptr<INotificationServiceClient> notifications =
      test::sharedMock(notificationMock);
  TelemetryService service{repository, devices, users, notifications, 30.0};

  int savedDeviceId{-1};
  double savedTemperature{0.0};
  int saves{0};
  int reads{0};
  int deletes{0};
  std::string repositoryFailure;
  bool deferSave{false};
  std::function<void()> pendingSaveSuccess;
  std::function<void(const std::string &)> pendingSaveError;

  int requestedDeviceId{-1};
  int deviceLookups{0};
  int ownerId{42};
  std::string deviceName{"Living room"};
  std::string deviceFailure;

  int requestedUserId{-1};
  int userLookups{0};
  std::string chatId{"chat-77"};
  std::string userFailure;

  std::string notificationChatId;
  std::string notificationDeviceName;
  double notificationTemperature{0.0};
  double notificationThreshold{0.0};
  int notificationCalls{0};
  std::string notificationFailure;

  TelemetryFixture() {
    fakeit::When(Method(*repositoryMock, save))
        .AlwaysDo([this](int id, double temperature,
                         std::function<void()> success,
                         std::function<void(const std::string &)> error) {
          savedDeviceId = id;
          savedTemperature = temperature;
          ++saves;
          if (deferSave) {
            pendingSaveSuccess = std::move(success);
            pendingSaveError = std::move(error);
          } else if (repositoryFailure.empty()) {
            success();
          } else {
            error(repositoryFailure);
          }
        });
    fakeit::When(Method(*repositoryMock, getByDeviceId))
        .AlwaysDo([this](int id,
                         std::function<void(const std::string &)> success,
                         std::function<void(const std::string &)> error) {
          savedDeviceId = id;
          ++reads;
          if (repositoryFailure.empty()) success("[telemetry]");
          else error(repositoryFailure);
        });
    fakeit::When(Method(*repositoryMock, deleteByDeviceId))
        .AlwaysDo([this](int id, std::function<void()> success,
                         std::function<void(const std::string &)> error) {
          savedDeviceId = id;
          ++deletes;
          if (repositoryFailure.empty()) success();
          else error(repositoryFailure);
        });
    fakeit::When(Method(*devicesMock, getNotificationInfo))
        .AlwaysDo([this](int id,
                         std::function<void(int, const std::string &)> success,
                         std::function<void(const std::string &)> error) {
          requestedDeviceId = id;
          ++deviceLookups;
          if (deviceFailure.empty()) success(ownerId, deviceName);
          else error(deviceFailure);
        });
    fakeit::When(Method(*usersMock, getTelegramChatId))
        .AlwaysDo([this](int id,
                         std::function<void(const std::string &)> success,
                         std::function<void(const std::string &)> error) {
          requestedUserId = id;
          ++userLookups;
          if (userFailure.empty()) success(chatId);
          else error(userFailure);
        });
    fakeit::When(Method(*notificationMock, sendTemperatureAlert))
        .AlwaysDo([this](const std::string &chat,
                         const std::string &device, double temperature,
                         double threshold, std::function<void()> success,
                         std::function<void(const std::string &)> error) {
          notificationChatId = chat;
          notificationDeviceName = device;
          notificationTemperature = temperature;
          notificationThreshold = threshold;
          ++notificationCalls;
          if (notificationFailure.empty()) success();
          else error(notificationFailure);
        });
  }

  /// Завершает сохранение, callback которого был удержан тестом.
  void completeDeferredSave() {
    if (repositoryFailure.empty()) {
      if (pendingSaveSuccess) pendingSaveSuccess();
    } else if (pendingSaveError) {
      pendingSaveError(repositoryFailure);
    }
    pendingSaveSuccess = {};
    pendingSaveError = {};
  }
};

}  // namespace

/// При безопасной температуре сервис сохраняет запись, но не ищет получателя.
TEST_CASE(telemetry_below_or_equal_threshold_does_not_lookup_or_notify) {
  TelemetryFixture fixture;
  std::string result;
  fixture.service.save(5, 30.0,
                       [&](const std::string &value) { result = value; },
                       [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("not_required", result);
  EXPECT_EQ(5, fixture.savedDeviceId);
  EXPECT_EQ(30.0, fixture.savedTemperature);
  EXPECT_EQ(1, fixture.saves);
  EXPECT_EQ(0, fixture.deviceLookups);
  EXPECT_EQ(0, fixture.userLookups);
  EXPECT_EQ(0, fixture.notificationCalls);
  fakeit::Verify(Method(*fixture.repositoryMock, save)).Once();
}

/// При превышении порога сервис проходит всю цепочку получения данных и уведомления.
TEST_CASE(telemetry_above_threshold_resolves_user_and_sends_notification) {
  TelemetryFixture fixture;
  std::string result;
  fixture.service.save(8, 31.25,
                       [&](const std::string &value) { result = value; },
                       [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("sent", result);
  EXPECT_EQ(8, fixture.requestedDeviceId);
  EXPECT_EQ(42, fixture.requestedUserId);
  EXPECT_EQ("chat-77", fixture.notificationChatId);
  EXPECT_EQ("Living room", fixture.notificationDeviceName);
  EXPECT_EQ(31.25, fixture.notificationTemperature);
  EXPECT_EQ(30.0, fixture.notificationThreshold);
  EXPECT_EQ(1, fixture.notificationCalls);
  fakeit::Verify(Method(*fixture.devicesMock, getNotificationInfo)).Once();
  fakeit::Verify(Method(*fixture.usersMock, getTelegramChatId)).Once();
  fakeit::Verify(Method(*fixture.notificationMock, sendTemperatureAlert)).Once();
}

/// Пустой Telegram chat ID отключает уведомление без ошибки сохранения.
TEST_CASE(telemetry_skips_notification_when_user_has_no_chat_id) {
  TelemetryFixture fixture;
  fixture.chatId.clear();
  std::string result;
  fixture.service.save(9, 35.0,
                       [&](const std::string &value) { result = value; },
                       [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("no_chat_id", result);
  EXPECT_EQ(1, fixture.deviceLookups);
  EXPECT_EQ(1, fixture.userLookups);
  EXPECT_EQ(0, fixture.notificationCalls);
}

/// Ошибка поиска устройства или пользователя завершает цепочку уведомления.
TEST_CASE(telemetry_reports_device_and_user_lookup_failures) {
  TelemetryFixture deviceFailure;
  deviceFailure.deviceFailure = "device missing";
  std::string deviceResult;
  deviceFailure.service.save(4, 40.0,
      [&](const std::string &value) { deviceResult = value; },
      [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("device_lookup_failed: device missing", deviceResult);
  EXPECT_EQ(0, deviceFailure.userLookups);

  TelemetryFixture userFailure;
  userFailure.userFailure = "user missing";
  std::string userResult;
  userFailure.service.save(4, 40.0,
      [&](const std::string &value) { userResult = value; },
      [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("user_lookup_failed: user missing", userResult);
  EXPECT_EQ(0, userFailure.notificationCalls);
}

/// Ошибки доставки становятся статусом уведомления, а ошибка БД идёт в error callback.
TEST_CASE(telemetry_reports_notification_and_repository_failures) {
  TelemetryFixture notificationFailure;
  notificationFailure.notificationFailure = "HTTP 502";
  std::string notificationResult;
  notificationFailure.service.save(4, 40.0,
      [&](const std::string &value) { notificationResult = value; },
      [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("delivery_failed: HTTP 502", notificationResult);

  TelemetryFixture repositoryFailure;
  repositoryFailure.repositoryFailure = "database down";
  std::string error;
  std::string result;
  repositoryFailure.service.save(4, 40.0,
      [&](const std::string &value) { result = value; },
      [&](const std::string &value) { error = value; });
  EXPECT_EQ("database down", error);
  EXPECT_EQ("", result);
  EXPECT_EQ(0, repositoryFailure.deviceLookups);
}

/// Чтение и удаление истории напрямую передаются репозиторию.
TEST_CASE(telemetry_read_and_delete_are_forwarded_to_repository) {
  TelemetryFixture fixture;
  std::string body;
  fixture.service.getByDeviceId(14,
      [&](const std::string &value) { body = value; },
      [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("[telemetry]", body);
  EXPECT_EQ(14, fixture.savedDeviceId);
  EXPECT_EQ(1, fixture.reads);

  bool deleted = false;
  fixture.service.deleteByDeviceId(14, [&] { deleted = true; },
      [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(deleted);
  EXPECT_EQ(1, fixture.deletes);
}

/// Ошибка сохранения не запускает поиск устройства, пользователя и уведомление.
TEST_CASE(telemetry_repository_failure_stops_notification_pipeline) {
  TelemetryFixture fixture;
  fixture.repositoryFailure = "database unavailable";
  int successCalls = 0;
  int errorCalls = 0;
  std::string error;

  fixture.service.save(7, 55.0,
      [&](const std::string &) { ++successCalls; },
      [&](const std::string &value) { ++errorCalls; error = value; });

  EXPECT_EQ(0, successCalls);
  EXPECT_EQ(1, errorCalls);
  EXPECT_EQ("database unavailable", error);
  EXPECT_EQ(0, fixture.deviceLookups);
  EXPECT_EQ(0, fixture.userLookups);
  EXPECT_EQ(0, fixture.notificationCalls);
}

/// Проверяет отложенный callback репозитория и асинхронное продолжение цепочки.
TEST_CASE(telemetry_pipeline_can_complete_after_save_returns) {
  TelemetryFixture fixture;
  fixture.deferSave = true;
  int completionCalls = 0;
  std::string result;
  fixture.service.save(18, 31.0,
      [&](const std::string &value) { ++completionCalls; result = value; },
      [](const std::string &) { EXPECT_TRUE(false); });

  EXPECT_EQ(0, completionCalls);
  EXPECT_EQ(0, fixture.deviceLookups);
  fixture.completeDeferredSave();

  EXPECT_EQ(1, completionCalls);
  EXPECT_EQ("sent", result);
  EXPECT_EQ(1, fixture.deviceLookups);
  EXPECT_EQ(1, fixture.userLookups);
  EXPECT_EQ(1, fixture.notificationCalls);
}

/// Сбой отправки уведомления не вызывает error callback самого сохранения.
TEST_CASE(telemetry_success_callback_is_called_once_for_notification_failure) {
  TelemetryFixture fixture;
  fixture.notificationFailure = "HTTP 502";
  int successCalls = 0;
  int errorCalls = 0;
  std::string result;
  fixture.service.save(19, 45.0,
      [&](const std::string &value) { ++successCalls; result = value; },
      [&](const std::string &) { ++errorCalls; });

  EXPECT_EQ(1, successCalls);
  EXPECT_EQ(0, errorCalls);
  EXPECT_EQ("delivery_failed: HTTP 502", result);
}

/// Чтение и удаление передают ошибки репозитория без изменения.
TEST_CASE(telemetry_read_and_delete_propagate_repository_errors) {
  TelemetryFixture fixture;
  fixture.repositoryFailure = "read-only database";

  std::string readError;
  int readSuccessCalls = 0;
  fixture.service.getByDeviceId(21,
      [&](const std::string &) { ++readSuccessCalls; },
      [&](const std::string &value) { readError = value; });
  EXPECT_EQ(0, readSuccessCalls);
  EXPECT_EQ("read-only database", readError);

  std::string deleteError;
  int deleteSuccessCalls = 0;
  fixture.service.deleteByDeviceId(21,
      [&] { ++deleteSuccessCalls; },
      [&](const std::string &value) { deleteError = value; });
  EXPECT_EQ(0, deleteSuccessCalls);
  EXPECT_EQ("read-only database", deleteError);
}

/// Сервис без зависимостей уведомлений отклоняет сохранение до обращения к БД.
TEST_CASE(telemetry_save_reports_missing_notification_dependencies) {
  auto repositoryMock = std::make_shared<fakeit::Mock<ITelemetryRepository>>();
  TelemetryService service(test::sharedMock(repositoryMock));
  std::string error;
  int successCalls = 0;

  service.save(5, 99.0,
      [&](const std::string &) { ++successCalls; },
      [&](const std::string &value) { error = value; });

  EXPECT_EQ(0, successCalls);
  EXPECT_EQ("TelemetryService notification dependencies are not configured", error);
  fakeit::Verify(Method(*repositoryMock, save)).Never();
}

/// Конструктор проверяет наличие репозитория и всех зависимостей уведомлений.
TEST_CASE(telemetry_constructor_rejects_null_dependencies) {
  bool repositoryRejected = false;
  try {
    TelemetryService service(std::shared_ptr<ITelemetryRepository>{});
  } catch (const std::invalid_argument &) {
    repositoryRejected = true;
  }
  EXPECT_TRUE(repositoryRejected);

  auto repositoryMock = std::make_shared<fakeit::Mock<ITelemetryRepository>>();
  auto devicesMock = std::make_shared<fakeit::Mock<IDevicesService>>();
  auto usersMock = std::make_shared<fakeit::Mock<IUsersService>>();
  bool dependenciesRejected = false;
  try {
    TelemetryService service(
        test::sharedMock(repositoryMock), test::sharedMock(devicesMock),
        test::sharedMock(usersMock),
        std::shared_ptr<INotificationServiceClient>{}, 30.0);
  } catch (const std::invalid_argument &) {
    dependenciesRejected = true;
  }
  EXPECT_TRUE(dependenciesRejected);
}
