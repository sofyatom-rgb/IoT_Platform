#pragma once

#include <string>

// Хеширование паролей: PBKDF2-HMAC-SHA256 с солью и перцем.
class PasswordHasher {
 public:
  explicit PasswordHasher(std::string pepper);

  // Случайная соль (hex-строка), уникальная для каждого пользователя.
  static std::string generateSalt();

  // Хеш (hex) от пароля, соли и перца.
  std::string hash(const std::string& password, const std::string& salt) const;

  // Сравнивает пароль с сохранённым хешем за постоянное время.
  bool verify(const std::string& password, const std::string& salt,
              const std::string& expectedHash) const;

 private:
  std::string pepper_;
};