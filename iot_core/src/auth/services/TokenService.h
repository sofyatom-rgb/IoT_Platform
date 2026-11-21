#pragma once

#include <optional>
#include <string>

// Выпуск и проверка JWT (HS256). Секрет берётся из JWT_SECRET.
class TokenService {
 public:
  // Создаёт токен на 30 минут для пользователя.
  static std::string create(int userId);

  // Возвращает user_id или nullopt, если токен неверный или просрочен.
  static std::optional<int> verify(const std::string &token);

 private:
  static std::string secret();
};