#include "TestFramework.h"

#include "users/services/UsersService.h"

#include <memory>

namespace {
class FakeUsersRepository final : public IUsersRepository {
 public:
  mutable int lastUserId{-1};
  mutable std::string email, password, name, chatId;
  mutable std::string response{"user-json"};
  mutable std::string failure;

  void save(const std::string &e, const std::string &p, const std::string &n,
            const std::string &chat, std::function<void(int)> success,
            std::function<void(const std::string &)> error) const override {
    email = e; password = p; name = n; chatId = chat;
    if (failure.empty()) success(71); else error(failure);
  }
  void deleteById(int id, std::function<void()> success,
                  std::function<void(const std::string &)> error) const override {
    lastUserId = id;
    if (failure.empty()) success(); else error(failure);
  }
  void getTelegramChatId(int id, std::function<void(const std::string &)> success,
                         std::function<void(const std::string &)> error) const override {
    lastUserId = id;
    if (failure.empty()) success(chatId); else error(failure);
  }
  void updateById(int id, const std::string &e, const std::string &p,
                  const std::string &n, const std::string &chat,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const override {
    lastUserId = id; email = e; password = p; name = n; chatId = chat;
    if (failure.empty()) success(); else error(failure);
  }
  void getById(int id, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) const override {
    lastUserId = id;
    if (failure.empty()) success(response); else error(failure);
  }
};
}  // namespace

TEST_CASE(users_service_passes_create_arguments_and_result) {
  auto repository = std::make_shared<FakeUsersRepository>();
  UsersService service(repository);
  int createdId = -1;
  service.save("a@example.org", "secret", "Alice", "9988",
               [&](int id) { createdId = id; },
               [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ(71, createdId);
  EXPECT_EQ("a@example.org", repository->email);
  EXPECT_EQ("secret", repository->password);
  EXPECT_EQ("Alice", repository->name);
  EXPECT_EQ("9988", repository->chatId);
}

TEST_CASE(users_service_forwards_queries_and_errors) {
  auto repository = std::make_shared<FakeUsersRepository>();
  UsersService service(repository);
  std::string chatId;
  service.getTelegramChatId(17, [&](const std::string &value) { chatId = value; },
                            [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ(17, repository->lastUserId);
  EXPECT_EQ("", chatId);

  repository->failure = "database unavailable";
  std::string observedError;
  service.getById(18, [](const std::string &) { EXPECT_TRUE(false); },
                  [&](const std::string &error) { observedError = error; });
  EXPECT_EQ(18, repository->lastUserId);
  EXPECT_EQ("database unavailable", observedError);
}

TEST_CASE(users_service_forwards_delete_and_update_operations) {
  auto repository = std::make_shared<FakeUsersRepository>();
  UsersService service(repository);
  bool deleted = false;
  service.deleteById(21, [&] { deleted = true; },
                     [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(deleted);
  EXPECT_EQ(21, repository->lastUserId);

  bool updated = false;
  service.updateById(22, "b@example.org", "hash", "Bob", "1234",
                     [&] { updated = true; },
                     [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(updated);
  EXPECT_EQ(22, repository->lastUserId);
  EXPECT_EQ("b@example.org", repository->email);
  EXPECT_EQ("Bob", repository->name);
  EXPECT_EQ("1234", repository->chatId);
}
