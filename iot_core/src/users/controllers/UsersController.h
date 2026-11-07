#pragma once

#include <drogon/drogon.h>

#include <memory>

#include "users/services/IUsersService.h"

// HTTP-слой пользователей: принимает JSON и отправляет в сервис.
class UsersController {
 public:
  explicit UsersController(std::shared_ptr<IUsersService> service);

  void registerRoutes();

  void create(const drogon::HttpRequestPtr &request,
              std::function<void(const drogon::HttpResponsePtr &)> callback);

  void deleteUser(const drogon::HttpRequestPtr &request,
                  std::function<void(const drogon::HttpResponsePtr &)> callback);

  void updateUser(const drogon::HttpRequestPtr &request,
                  std::function<void(const drogon::HttpResponsePtr &)> callback);

  void getUser(const drogon::HttpRequestPtr &request,
               std::function<void(const drogon::HttpResponsePtr &)> callback);

 private:
  std::shared_ptr<IUsersService> service_;
};