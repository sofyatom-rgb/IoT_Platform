#pragma once

#include <drogon/drogon.h>

#include "users/services/UsersService.h"

// HTTP-слой пользователей: принимает JSON и отправляет в сервис.
class UsersController {
 public:
  void registerRoutes();

  void create(const drogon::HttpRequestPtr &request,
              std::function<void(const drogon::HttpResponsePtr &)> callback);

  void deleteUser(const drogon::HttpRequestPtr &request,
                  std::function<void(const drogon::HttpResponsePtr &)> callback);
};