#pragma once

#include <string>

/// Хеширует пароли алгоритмом PBKDF2-HMAC-SHA256 с солью и pepper.
class PasswordHasher {
 public:
  /// Создаёт хешер с секретным pepper.
  explicit PasswordHasher(std::string pepper);

  /// Генерирует случайную соль в шестнадцатеричном виде.
  static std::string generateSalt();

  /// Вычисляет хеш пароля с солью и секретным pepper.
  std::string hash(const std::string& password, const std::string& salt) const;

  /// Сравнивает пароль с сохранённым хешем за постоянное время.
  bool verify(const std::string& password, const std::string& salt,
              const std::string& expectedHash) const;

 private:
  std::string pepper_;
};