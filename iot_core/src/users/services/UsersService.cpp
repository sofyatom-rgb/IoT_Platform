#include "users/services/UsersService.h"

#include <utility>

UsersService::UsersService(drogon::orm::DbClientPtr db)
    : repository_(std::move(db)) {}

void UsersService::save(const std::string &email,
                        const std::string &password,
                        const std::string &name,
                        std::function<void(int)> success,
                        std::function<void(const std::string &)> error) const {
  repository_.save(email, password, name, std::move(success), std::move(error));
}