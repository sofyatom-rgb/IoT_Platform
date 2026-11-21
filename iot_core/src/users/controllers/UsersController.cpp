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

  // PUT /users?id=X -> обновить пользователя.
  drogon::app().registerHandler(
      "/users",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        updateUser(request, std::move(callback));
      },
      {drogon::Put});

  // GET /users?id=X -> получить пользователя.
  drogon::app().registerHandler(
      "/users",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        getUser(request, std::move(callback));
      },
      {drogon::Get});
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

void UsersController::updateUser(
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

  std::string userIdStr = request->getParameter("id");
  if (userIdStr.empty()) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Missing id parameter";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  int userId = std::stoi(userIdStr);
  std::string email = (*json)["email"].asString();
  std::string password = (*json)["password"].asString();
  std::string name = (*json)["name"].asString();
  std::string telegramChatId;
  if ((*json).isMember("telegram_chat_id") && !(*json)["telegram_chat_id"].isNull()) {
    telegramChatId = (*json)["telegram_chat_id"].asString();
  }

  UsersService service(drogon::app().getDbClient());
  service.updateById(
      userId, email, password, name, telegramChatId,
      [callback]() {
        Json::Value body;
        body["status"] = "ok";
        body["message"] = "User updated";
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}

void UsersController::getUser(
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
  service.getById(
      userId,
      [callback](const std::string &responseBody) {
        Json::Value body;
        Json::CharReaderBuilder builder;
        std::string errs;
        std::istringstream iss(responseBody);
        if (Json::parseFromStream(builder, iss, &body, &errs)) {
          callback(drogon::HttpResponse::newHttpJsonResponse(body));
        } else {
          Json::Value errorBody;
          errorBody["status"] = "error";
          errorBody["message"] = "Failed to parse user data";
          callback(drogon::HttpResponse::newHttpJsonResponse(errorBody));
        }
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}