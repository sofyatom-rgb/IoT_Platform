#include "auth/services/AuthService.h"

#include <cstdlib>
#include <stdexcept>

#include "auth/services/TokenService.h"

namespace {
std::string readPepper() {
  const char *p = std::getenv("PASSWORD_PEPPER");
  if (!p || std::string(p).empty())
    throw std::runtime_error("PASSWORD_PEPPER is not set");
  return p;
}
}  // namespace

AuthService::AuthService(drogon::orm::DbClientPtr db)
    : repository_(std::move(db)), hasher_(readPepper()) {}

void AuthService::login(const std::string &email, const std::string &password,
                        std::function<void(const std::string &, int)> success,
                        std::function<void()> invalid,
                        std::function<void(const std::string &)> error) const {
  // hasher_ копируем: колбэк выполнится, когда сервис уже может быть удалён
  auto hasher = hasher_;
  repository_.findCredentials(
      email,
      [=](std::optional<AuthRepository::Credentials> c) {
        if (!c || !hasher.verify(password, c->salt, c->passwordHash)) {
          invalid();
          return;
        }
        success(TokenService::create(c->id), c->id);
      },
      error);
}