#include "users/services/UsersService.h"

#include "users/repositories/UsersRepository.h"

#include <utility>

UsersService::UsersService(std::shared_ptr<drogon::orm::DbClient> db)
    : UsersService(std::make_shared<UsersRepository>(std::move(db))) {}
