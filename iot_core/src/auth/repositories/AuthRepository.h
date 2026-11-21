#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <optional>
#include <string>

// Репозиторий входа: SQL для проверки учётных данных.
class AuthRepository {
 public:
  struct Credentials {
    int id;
    std::string passwordHash;
    std::string salt;
  };

  explicit AuthRepository(drogon::orm::DbClientPtr db);

  void findCredentials(const std::string &email,
                       std::function<void(std::optional<Credentials>)> success,
                       std::function<void(const std::string &)> error) const;

 private:
  drogon::orm::DbClientPtr db_;
};