#pragma once

#include <functional>
#include <memory>
#include <string>
#include "services/PasswordHasher.h"
#include "users/repositories/IUsersRepository.h"

namespace drogon::orm { class DbClient; }

/// Выполняет SQL-запросы к таблице пользователей.
class UsersRepository : public IUsersRepository {
 public:
  /// Создаёт репозиторий с подключением к PostgreSQL.
  explicit UsersRepository(std::shared_ptr<drogon::orm::DbClient> db);

  /// Добавляет пользователя с хешем пароля.
  void save(const std::string &email,
            const std::string &password,
            const std::string &name,
            const std::string &telegramChatId,
            std::function<void(int)> success,
            std::function<void(const std::string &)> error) const override;

  /// Удаляет пользователя по ID.
  void deleteById(int userId,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const override;

  /// Читает Telegram chat ID пользователя.
  void getTelegramChatId(
      int userId,
      std::function<void(const std::string &)> success,
      std::function<void(const std::string &)> error) const override;

  /// Обновляет данные пользователя и пересчитывает хеш пароля.
  void updateById(int userId,
                  const std::string &email,
                  const std::string &password,
                  const std::string &name,
                  const std::string &telegramChatId,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const override;

  /// Читает основные поля пользователя в JSON.
  void getById(int userId,
               std::function<void(const std::string &)> success,
               std::function<void(const std::string &)> error) const override;

 private:
  /// Читает секретный pepper из переменной окружения.
  static std::string readPepper();
  PasswordHasher hasher_;
  std::shared_ptr<drogon::orm::DbClient> db_;
};
