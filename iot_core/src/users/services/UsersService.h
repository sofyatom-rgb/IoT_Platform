#pragma once

#include <functional>
#include <memory>
#include <string>

#include "users/repositories/IUsersRepository.h"
#include "users/services/IUsersService.h"

namespace drogon::orm { class DbClient; }

/// Сервис пользовательских операций; SQL выполняет репозиторий.
class UsersService : public IUsersService {
 public:
  /// Создаёт сервис с PostgreSQL-репозиторием.
  explicit UsersService(std::shared_ptr<drogon::orm::DbClient> db);

  /// Создаёт сервис с переданным репозиторием.
  explicit UsersService(std::shared_ptr<IUsersRepository> repository);

  /// Регистрирует пользователя и возвращает ID новой записи.
  void save(const std::string &email, const std::string &password,
            const std::string &name, const std::string &telegramChatId,
            std::function<void(int)> success,
            std::function<void(const std::string &)> error) const;

  /// Удаляет пользователя.
  void deleteById(int userId, std::function<void()> success,
                  std::function<void(const std::string &)> error) const;

  /// Возвращает Telegram chat ID пользователя.
  void getTelegramChatId(
      int userId, std::function<void(const std::string &)> success,
      std::function<void(const std::string &)> error) const override;

  /// Обновляет данные пользователя.
  void updateById(int userId, const std::string &email,
                  const std::string &password, const std::string &name,
                  const std::string &telegramChatId,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const;

  /// Возвращает данные пользователя в JSON.
  void getById(int userId, std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) const;

 private:
  std::shared_ptr<IUsersRepository> repository_;
};
