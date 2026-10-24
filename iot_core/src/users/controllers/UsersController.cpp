#include "users/controllers/UsersController.h"

#include <string>
#include <utility>

void UsersController::registerRoutes() {
  // POST /users -> создать пользователя.
  drogon::app().registerHandler(
      "/users",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        create(request, std::move(callback));
      },
      {drogon::Post});

  // DELETE /users?id=X -> удалить пользователя и все его устройства/telemetry.
  drogon::app().registerHandler(
      "/users",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        deleteUser(request, std::move(callback));
      },
      {drogon::Delete});
}

void UsersController::create(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();

  if (!json) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Invalid JSON";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  UsersService service(drogon::app().getDbClient());
  service.save(
      (*json)["email"].asString(),
      (*json)["password"].asString(),
      (*json)["name"].asString(),
      [callback](int userId) {
        Json::Value body;
        body["status"] = "ok";
        body["user_id"] = userId;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}

void UsersController::deleteUser(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  std::string userIdStr = request->getParameter("id");
  if (userIdStr.empty()) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Missing id parameter";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  int userId = std::stoi(userIdStr);

  UsersService service(drogon::app().getDbClient());
  service.deleteById(
      userId,
      [callback]() {
        Json::Value body;
        body["status"] = "ok";
        body["message"] = "User deleted";
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}