#pragma once

#include <functional>
#include <string>

#include "users/repositories/UsersRepository.h"

// Сервисный слой пользователей.
class UsersService {
 public:
  explicit UsersService(drogon::orm::DbClientPtr db);

  void save(const std::string &email,
            const std::string &password,
            const std::string &name,
            const std::string &telegramChatId,
            std::function<void(int)> success,
            std::function<void(const std::string &)> error) const;

  void deleteById(int userId,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const;

  void getTelegramChatId(
      int userId,
      std::function<void(const std::string &)> success,
      std::function<void(const std::string &)> error) const;

 private:
  UsersRepository repository_;
};