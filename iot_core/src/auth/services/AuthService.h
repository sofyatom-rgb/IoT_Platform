#pragma once

#include <functional>
#include <string>

#include "auth/repositories/AuthRepository.h"
#include "services/PasswordHasher.h"

// Сервис входа: проверка пароля и выпуск токена.
class AuthService {
 public:
  explicit AuthService(drogon::orm::DbClientPtr db);

  // success получает токен и user_id; invalid вызывается при неверных данных.
  void login(const std::string &email, const std::string &password,
             std::function<void(const std::string &, int)> success,
             std::function<void()> invalid,
             std::function<void(const std::string &)> error) const;

 private:
  AuthRepository repository_;
  PasswordHasher hasher_;
};