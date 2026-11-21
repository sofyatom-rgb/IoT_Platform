#include "auth/repositories/AuthRepository.h"

#include <utility>

AuthRepository::AuthRepository(drogon::orm::DbClientPtr db)
    : db_(std::move(db)) {}

void AuthRepository::findCredentials(
    const std::string &email,
    std::function<void(std::optional<Credentials>)> success,
    std::function<void(const std::string &)> error) const {
  db_->execSqlAsync(
      "SELECT id, password_hash, salt FROM users WHERE email = $1",
      [success](const drogon::orm::Result &r) {
        if (r.empty()) {
          success(std::nullopt);
          return;
        }
        success(Credentials{r[0]["id"].as<int>(),
                            r[0]["password_hash"].as<std::string>(),
                            r[0]["salt"].as<std::string>()});
      },
      [error](const drogon::orm::DrogonDbException &e) {
        error(e.base().what());
      },
      email);
}