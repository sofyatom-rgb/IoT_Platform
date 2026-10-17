#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>

// Репозиторий пользователей: единственное место, где выполняется SQL для users.
class UsersRepository {
 public:
  explicit UsersRepository(drogon::orm::DbClientPtr db);

  void save(const std::string &email,
            const std::string &password,
            const std::string &name,
            std::function<void(int)> success,
            std::function<void(const std::string &)> error) const;

 private:
  drogon::orm::DbClientPtr db_;
};