#include "users/controllers/UsersController.h"

#include <cctype>
#include <string>
#include <utility>

namespace {
bool getTelegramChatId(const Json::Value &json, std::string &chatId) {
  if (!json.isMember("telegram_chat_id") || json["telegram_chat_id"].isNull()) {
    return true;
  }

  const auto &value = json["telegram_chat_id"];
  if (value.isString()) {
    chatId = value.asString();
  } else if (value.isInt64()) {
    chatId = std::to_string(value.asInt64());
  } else {
    return false;
  }

  if (chatId.empty()) {
    return true;
  }

  std::size_t firstDigit = chatId.front() == '-' ? 1 : 0;
  if (firstDigit == chatId.size()) {
    return false;
  }
  for (std::size_t i = firstDigit; i < chatId.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(chatId[i]))) {
      return false;
    }
  }
  return true;
}
}  // namespace

void UsersController::registerRoutes() {
  // POST /users -> создать пользователя.
  drogon::app().registerHandler(
      "/users",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        create(request, std::move(callback));
      },
      {drogon::Post});

  // DELETE /users with JSON {"user_id": X} -> удалить пользователя.
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

  std::string telegramChatId;
  if (!getTelegramChatId(*json, telegramChatId)) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "telegram_chat_id must be an integer or numeric string";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  UsersService service(drogon::app().getDbClient());
  service.save(
      (*json)["email"].asString(),
      (*json)["password"].asString(),
      (*json)["name"].asString(),
      telegramChatId,
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
  const auto json = request->getJsonObject();
  if (!json || !(*json)["user_id"].isInt() || (*json)["user_id"].asInt() <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive integer user_id in JSON body";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  const int userId = (*json)["user_id"].asInt();

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