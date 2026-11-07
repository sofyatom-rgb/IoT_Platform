#pragma once

#include <functional>
#include <memory>
#include <string>

#include "users/repositories/IUsersRepository.h"
#include "users/services/IUsersService.h"

// Сервисный слой пользователей. Репозиторий внедряется через интерфейс,
// поэтому бизнес-логику/делегирование можно тестировать без PostgreSQL.
class UsersService : public IUsersService {
 public:
  explicit UsersService(std::shared_ptr<IUsersRepository> repository);

  void save(const std::string &email, const std::string &password,
            const std::string &name, const std::string &telegramChatId,
            std::function<void(int)> success,
            std::function<void(const std::string &)> error) const override;
  void deleteById(int userId, std::function<void()> success,
                  std::function<void(const std::string &)> error) const override;
  void getTelegramChatId(int userId,
                         std::function<void(const std::string &)> success,
                         std::function<void(const std::string &)> error) const override;
  void updateById(int userId, const std::string &email,
                  const std::string &password, const std::string &name,
                  const std::string &telegramChatId, std::function<void()> success,
                  std::function<void(const std::string &)> error) const override;
  void getById(int userId, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) const override;

 private:
  std::shared_ptr<IUsersRepository> repository_;
};
