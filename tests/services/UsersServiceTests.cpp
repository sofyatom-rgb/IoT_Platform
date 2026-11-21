#include "TestFramework.h"
#include "TestSupport.h"

#include <fakeit.hpp>
#include "users/services/UsersService.h"

#include <memory>
#include <string>

/// Проверяет передачу полей пользователя и получение ID от репозитория.
TEST_CASE(users_service_passes_create_arguments_and_result) {
  auto repositoryMock = std::make_shared<fakeit::Mock<IUsersRepository>>();
  auto repository = test::sharedMock(repositoryMock);
  std::string email, password, name, chatId;

  fakeit::When(Method(*repositoryMock, save))
      .Do([&](const std::string &receivedEmail,
              const std::string &receivedPassword,
              const std::string &receivedName,
              const std::string &receivedChatId,
              std::function<void(int)> success,
              std::function<void(const std::string &)>) {
        email = receivedEmail;
        password = receivedPassword;
        name = receivedName;
        chatId = receivedChatId;
        success(71);
      });

  UsersService service(repository);
  int createdId = -1;
  service.save("a@example.org", "secret", "Alice", "9988",
               [&](int id) { createdId = id; },
               [](const std::string &) { EXPECT_TRUE(false); });

  EXPECT_EQ(71, createdId);
  EXPECT_EQ("a@example.org", email);
  EXPECT_EQ("secret", password);
  EXPECT_EQ("Alice", name);
  EXPECT_EQ("9988", chatId);
  fakeit::Verify(Method(*repositoryMock, save)).Once();
}

/// Проверяет передачу запросов и ошибок репозитория вызывающему коду.
TEST_CASE(users_service_forwards_queries_and_errors) {
  auto repositoryMock = std::make_shared<fakeit::Mock<IUsersRepository>>();
  auto repository = test::sharedMock(repositoryMock);
  int requestedId = -1;
  std::string observedChatId;

  fakeit::When(Method(*repositoryMock, getTelegramChatId))
      .Do([&](int id, std::function<void(const std::string &)> success,
              std::function<void(const std::string &)>) {
        requestedId = id;
        success("");
      });
  fakeit::When(Method(*repositoryMock, getById))
      .Do([&](int id, std::function<void(const std::string &)>,
              std::function<void(const std::string &)> error) {
        requestedId = id;
        error("database unavailable");
      });

  UsersService service(repository);
  service.getTelegramChatId(17,
      [&](const std::string &value) { observedChatId = value; },
      [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ(17, requestedId);
  EXPECT_EQ("", observedChatId);

  std::string observedError;
  service.getById(18, [](const std::string &) { EXPECT_TRUE(false); },
                  [&](const std::string &error) { observedError = error; });
  EXPECT_EQ(18, requestedId);
  EXPECT_EQ("database unavailable", observedError);
  fakeit::Verify(Method(*repositoryMock, getTelegramChatId)).Once();
  fakeit::Verify(Method(*repositoryMock, getById)).Once();
}

/// Проверяет передачу операций удаления и обновления репозиторию.
TEST_CASE(users_service_forwards_delete_and_update_operations) {
  auto repositoryMock = std::make_shared<fakeit::Mock<IUsersRepository>>();
  auto repository = test::sharedMock(repositoryMock);
  int requestedId = -1;
  std::string email, password, name, chatId;

  fakeit::When(Method(*repositoryMock, deleteById))
      .Do([&](int id, std::function<void()> success,
              std::function<void(const std::string &)>) {
        requestedId = id;
        success();
      });
  fakeit::When(Method(*repositoryMock, updateById))
      .Do([&](int id, const std::string &receivedEmail,
              const std::string &receivedPassword,
              const std::string &receivedName,
              const std::string &receivedChatId,
              std::function<void()> success,
              std::function<void(const std::string &)>) {
        requestedId = id;
        email = receivedEmail;
        password = receivedPassword;
        name = receivedName;
        chatId = receivedChatId;
        success();
      });

  UsersService service(repository);
  bool deleted = false;
  service.deleteById(21, [&] { deleted = true; },
                     [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(deleted);
  EXPECT_EQ(21, requestedId);

  bool updated = false;
  service.updateById(22, "b@example.org", "hash", "Bob", "1234",
                     [&] { updated = true; },
                     [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(updated);
  EXPECT_EQ(22, requestedId);
  EXPECT_EQ("b@example.org", email);
  EXPECT_EQ("hash", password);
  EXPECT_EQ("Bob", name);
  EXPECT_EQ("1234", chatId);
  fakeit::Verify(Method(*repositoryMock, deleteById)).Once();
  fakeit::Verify(Method(*repositoryMock, updateById)).Once();
}
