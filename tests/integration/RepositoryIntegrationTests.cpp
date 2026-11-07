#include "TestFramework.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <drogon/orm/DbClient.h>
#include <json/json.h>

#include "devices/repositories/DevicesRepository.h"
#include "telemetry/repositories/TelemetryRepository.h"
#include "users/repositories/UsersRepository.h"

namespace {
using drogon::orm::DbClient;
using drogon::orm::DbClientPtr;

DbClientPtr database() {
  static DbClientPtr client = [] {
    const char *configured = std::getenv("IOT_TEST_DATABASE_URL");
    const std::string connection = configured && *configured
        ? configured
        : "host=127.0.0.1 port=5433 dbname=iot_platform_test user=postgres password=postgres";
    auto result = DbClient::newPgClient(connection, 2);
    // Fail early with a readable message if the test database is unavailable.
    result->execSqlSync("SELECT 1");
    return result;
  }();
  return client;
}

std::string uniqueEmail() {
  static std::atomic<unsigned long long> sequence{0};
  const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
  return "repository_test_" + std::to_string(ticks) + "_" +
         std::to_string(sequence.fetch_add(1)) + "@example.test";
}

template <typename T, typename Invoker>
T awaitValue(Invoker &&invoke) {
  auto promise = std::make_shared<std::promise<T>>();
  auto future = promise->get_future();
  invoke(
      [promise](T value) { promise->set_value(std::move(value)); },
      [promise](const std::string &message) {
        promise->set_exception(std::make_exception_ptr(std::runtime_error(message)));
      });
  if (future.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
    throw std::runtime_error("Timed out waiting for repository callback");
  }
  return future.get();
}

template <typename Invoker>
void awaitVoid(Invoker &&invoke) {
  auto promise = std::make_shared<std::promise<void>>();
  auto future = promise->get_future();
  invoke(
      [promise] { promise->set_value(); },
      [promise](const std::string &message) {
        promise->set_exception(std::make_exception_ptr(std::runtime_error(message)));
      });
  if (future.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
    throw std::runtime_error("Timed out waiting for repository callback");
  }
  future.get();
}

Json::Value parseJson(const std::string &text) {
  Json::CharReaderBuilder builder;
  Json::Value result;
  std::string error;
  std::istringstream input(text);
  if (!Json::parseFromStream(builder, input, &result, &error)) {
    throw std::runtime_error("Repository returned invalid JSON: " + error);
  }
  return result;
}

int createUser(UsersRepository &repository, const std::string &email) {
  return awaitValue<int>([&](auto success, auto error) {
    repository.save(email, "test-password", "Repository Test", "", success, error);
  });
}

int createDevice(DevicesRepository &repository, int ownerId,
                 const std::string &name = "Test sensor") {
  return awaitValue<int>([&](auto success, auto error) {
    repository.save(ownerId, name, "temperature", success, error);
  });
}

struct UserCleanup {
  int id{0};
  ~UserCleanup() {
    if (id > 0) {
      try { database()->execSqlSync("DELETE FROM users WHERE id = $1", id); }
      catch (...) { /* Preserve the original test failure. */ }
    }
  }
};
}  // namespace

TEST_CASE(users_repository_inserts_reads_chat_id_updates_and_deletes) {
  UsersRepository repository(database());
  const std::string email = uniqueEmail();
  UserCleanup cleanup{createUser(repository, email)};

  const Json::Value saved = parseJson(awaitValue<std::string>([&](auto success, auto error) {
    repository.getById(cleanup.id, success, error);
  }));
  EXPECT_EQ(cleanup.id, saved["id"].asInt());
  EXPECT_EQ(email, saved["email"].asString());
  EXPECT_EQ(std::string("Repository Test"), saved["name"].asString());

  awaitVoid([&](auto success, auto error) {
    repository.updateById(cleanup.id, email, "updated-password", "Updated name",
                          "-10012345", success, error);
  });

  const std::string chatId = awaitValue<std::string>([&](auto success, auto error) {
    repository.getTelegramChatId(cleanup.id, success, error);
  });
  EXPECT_EQ(std::string("-10012345"), chatId);

  awaitVoid([&](auto success, auto error) {
    repository.deleteById(cleanup.id, success, error);
  });
  cleanup.id = 0;
}

TEST_CASE(users_repository_reports_duplicate_email_constraint) {
  UsersRepository repository(database());
  const std::string email = uniqueEmail();
  UserCleanup cleanup{createUser(repository, email)};

  bool rejected = false;
  try {
    (void)awaitValue<int>([&](auto success, auto error) {
      repository.save(email, "another-password", "Duplicate", "", success, error);
    });
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  EXPECT_TRUE(rejected);
}

TEST_CASE(users_repository_returns_empty_chat_id_for_null_value) {
  UsersRepository repository(database());
  const std::string email = uniqueEmail();
  UserCleanup cleanup{createUser(repository, email)};

  const std::string chatId = awaitValue<std::string>([&](auto success, auto error) {
    repository.getTelegramChatId(cleanup.id, success, error);
  });
  EXPECT_EQ(std::string{}, chatId);
}

TEST_CASE(devices_repository_crud_and_notification_info_use_expected_columns) {
  UsersRepository users(database());
  DevicesRepository devices(database());
  const std::string email = uniqueEmail();
  UserCleanup cleanup{createUser(users, email)};
  const int deviceId = createDevice(devices, cleanup.id, "Garage thermometer");

  int ownerId = -1;
  std::string deviceName;
  awaitValue<int>([&](auto success, auto error) {
    devices.getNotificationInfo(
        deviceId,
        [&, success](int owner, const std::string &name) {
          ownerId = owner;
          deviceName = name;
          success(owner);
        },
        error);
  });
  EXPECT_EQ(cleanup.id, ownerId);
  EXPECT_EQ(std::string("Garage thermometer"), deviceName);

  awaitVoid([&](auto success, auto error) {
    devices.updateById(deviceId, cleanup.id, "Kitchen thermometer",
                       "temperature", success, error);
  });
  const Json::Value updated = parseJson(awaitValue<std::string>([&](auto success, auto error) {
    devices.getById(deviceId, success, error);
  }));
  EXPECT_EQ(std::string("Kitchen thermometer"), updated["name"].asString());

  awaitVoid([&](auto success, auto error) {
    devices.deleteById(deviceId, success, error);
  });
}

TEST_CASE(devices_repository_reports_missing_device_for_notification_lookup) {
  DevicesRepository devices(database());
  bool errorCalled = false;
  std::string message;
  auto promise = std::make_shared<std::promise<void>>();
  auto future = promise->get_future();
  devices.getNotificationInfo(
      2147483000,
      [promise](int, const std::string &) { promise->set_value(); },
      [&, promise](const std::string &error) {
        errorCalled = true;
        message = error;
        promise->set_value();
      });
  if (future.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
    throw std::runtime_error("Timed out waiting for missing-device lookup");
  }
  future.get();
  EXPECT_TRUE(errorCalled);
  EXPECT_EQ(std::string("Device not found"), message);
}

TEST_CASE(telemetry_repository_reads_latest_ten_rows_and_deletes_them) {
  UsersRepository users(database());
  DevicesRepository devices(database());
  TelemetryRepository telemetry(database());
  const std::string email = uniqueEmail();
  UserCleanup cleanup{createUser(users, email)};
  const int deviceId = createDevice(devices, cleanup.id, "Test telemetry device");

  for (int i = 0; i < 12; ++i) {
    awaitVoid([&](auto success, auto error) {
      telemetry.save(deviceId, static_cast<double>(i), success, error);
    });
  }

  const Json::Value rows = parseJson(awaitValue<std::string>([&](auto success, auto error) {
    telemetry.getByDeviceId(deviceId, success, error);
  }));
  EXPECT_EQ(10U, rows.size());
  EXPECT_EQ(11.0, rows[0]["temperature"].asDouble());
  EXPECT_EQ(2.0, rows[9]["temperature"].asDouble());

  awaitVoid([&](auto success, auto error) {
    telemetry.deleteByDeviceId(deviceId, success, error);
  });
  const Json::Value afterDelete = parseJson(awaitValue<std::string>([&](auto success, auto error) {
    telemetry.getByDeviceId(deviceId, success, error);
  }));
  EXPECT_EQ(0U, afterDelete.size());
}

TEST_CASE(telemetry_repository_routes_foreign_key_violation_to_error_callback) {
  TelemetryRepository telemetry(database());
  bool errorCalled = false;
  std::string message;
  auto promise = std::make_shared<std::promise<void>>();
  auto future = promise->get_future();
  telemetry.save(
      2147483000, 22.0,
      [promise] { promise->set_value(); },
      [&, promise](const std::string &error) {
        errorCalled = true;
        message = error;
        promise->set_value();
      });
  if (future.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
    throw std::runtime_error("Timed out waiting for foreign-key violation");
  }
  future.get();
  EXPECT_TRUE(errorCalled);
  EXPECT_FALSE(message.empty());
}

TEST_CASE(postgres_schema_cascades_user_deletion_to_devices_and_telemetry) {
  UsersRepository users(database());
  DevicesRepository devices(database());
  TelemetryRepository telemetry(database());
  const std::string email = uniqueEmail();
  UserCleanup cleanup{createUser(users, email)};
  const int deviceId = createDevice(devices, cleanup.id, "Cascade device");
  awaitVoid([&](auto success, auto error) {
    telemetry.save(deviceId, 33.0, success, error);
  });

  awaitVoid([&](auto success, auto error) {
    users.deleteById(cleanup.id, success, error);
  });
  cleanup.id = 0;

  const auto deviceCount = database()->execSqlSync(
      "SELECT COUNT(*) AS count FROM devices WHERE id = $1", deviceId);
  const auto telemetryCount = database()->execSqlSync(
      "SELECT COUNT(*) AS count FROM telemetry WHERE device_id = $1", deviceId);
  EXPECT_EQ(0, deviceCount[0]["count"].as<int>());
  EXPECT_EQ(0, telemetryCount[0]["count"].as<int>());
}
