#pragma once

#include <functional>
#include <string>
#include <utility>

struct NotificationTargets {
  std::string chatId;
  std::string email;
};

// Port репозитория пользователей. Не зависит от PostgreSQL/Drogon.
class IUsersRepository {
 public:
  virtual ~IUsersRepository() = default;

  virtual void save(const std::string &email,
                    const std::string &password,
                    const std::string &name,
                    const std::string &telegramChatId,
                    std::function<void(int)> success,
                    std::function<void(const std::string &)> error) const = 0;
  virtual void deleteById(int userId, std::function<void()> success,
                          std::function<void(const std::string &)> error) const = 0;
  virtual void getTelegramChatId(
      int userId, std::function<void(const std::string &)> success,
      std::function<void(const std::string &)> error) const = 0;
  virtual void getNotificationTargets(
      int userId, std::function<void(const NotificationTargets &)> success,
      std::function<void(const std::string &)> error) const {
    getTelegramChatId(
        userId,
        [success = std::move(success)](const std::string &chatId) {
          success(NotificationTargets{chatId, {}});
        },
        std::move(error));
  }
  virtual void updateById(int userId, const std::string &email,
                          const std::string &password, const std::string &name,
                          const std::string &telegramChatId,
                          std::function<void()> success,
                          std::function<void(const std::string &)> error) const = 0;
  virtual void getById(int userId,
                       std::function<void(const std::string &)> success,
                       std::function<void(const std::string &)> error) const = 0;
};
