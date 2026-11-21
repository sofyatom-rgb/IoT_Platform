#pragma once

#include <drogon/drogon.h>

#include <memory>

#include "users/services/IUsersService.h"

// HTTP-слой пользователей: принимает JSON и отправляет в сервис.
class UsersController {
 public:
  /// Создаёт контроллер с сервисом пользователей.
  explicit UsersController(std::shared_ptr<IUsersService> service);

  /// Регистрирует HTTP-маршруты пользователей.
  void registerRoutes();

  /// Создаёт пользователя из JSON-тела запроса.
  void create(const drogon::HttpRequestPtr &request,
              std::function<void(const drogon::HttpResponsePtr &)> callback);

  /// Удаляет пользователя по переданному ID.
  void deleteUser(const drogon::HttpRequestPtr &request,
                  std::function<void(const drogon::HttpResponsePtr &)> callback);

  /// Обновляет данные существующего пользователя.
  void updateUser(const drogon::HttpRequestPtr &request,
                  std::function<void(const drogon::HttpResponsePtr &)> callback);

  /// Возвращает данные пользователя по ID.
  void getUser(const drogon::HttpRequestPtr &request,
               std::function<void(const drogon::HttpResponsePtr &)> callback);

 private:
  std::shared_ptr<IUsersService> service_;
};