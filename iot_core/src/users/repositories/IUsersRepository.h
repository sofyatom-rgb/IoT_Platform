#pragma once

#include <functional>
#include <string>

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
  virtual void updateById(int userId, const std::string &email,
                          const std::string &password, const std::string &name,
                          const std::string &telegramChatId,
                          std::function<void()> success,
                          std::function<void(const std::string &)> error) const = 0;
  virtual void getById(int userId,
                       std::function<void(const std::string &)> success,
                       std::function<void(const std::string &)> error) const = 0;
};
