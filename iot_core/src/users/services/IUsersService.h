#pragma once

#include <functional>
#include <string>

/// Контракт пользовательских операций, доступных другим компонентам.
class IUsersService {
 public:
  /// Позволяет безопасно удалять реализацию через указатель на интерфейс.
  virtual ~IUsersService() = default;

  /// Создаёт пользователя и возвращает ID новой записи.
  virtual void save(const std::string &email, const std::string &password,
                    const std::string &name, const std::string &telegramChatId,
                    std::function<void(int)> success,
                    std::function<void(const std::string &)> error) const = 0;

  /// Удаляет пользователя по ID.
  virtual void deleteById(int userId, std::function<void()> success,
                          std::function<void(const std::string &)> error) const = 0;

  /// Возвращает Telegram chat ID пользователя.
  virtual void getTelegramChatId(
      int userId, std::function<void(const std::string &)> success,
      std::function<void(const std::string &)> error) const = 0;

  /// Обновляет данные пользователя.
  virtual void updateById(int userId, const std::string &email,
                          const std::string &password, const std::string &name,
                          const std::string &telegramChatId,
                          std::function<void()> success,
                          std::function<void(const std::string &)> error) const = 0;

  /// Возвращает данные пользователя в JSON.
  virtual void getById(int userId,
                       std::function<void(const std::string &)> success,
                       std::function<void(const std::string &)> error) const = 0;
};
