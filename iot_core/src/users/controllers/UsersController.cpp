#include "users/controllers/UsersController.h"

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