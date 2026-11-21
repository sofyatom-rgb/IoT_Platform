#include "users/controllers/UsersController.h"
#include "common/validation/RequestValidation.h"

#include <memory>
#include <stdexcept>

#include <sstream>
#include <string>
#include <utility>

UsersController::UsersController(std::shared_ptr<IUsersService> service)
    : service_(std::move(service)) {
  if (!service_) {
    throw std::invalid_argument("UsersController requires a users service");
  }
}

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

  return iot::validation::isValidOptionalTelegramChatId(chatId);
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

  // DELETE /users/{id} -> удалить пользователя.
  drogon::app().registerHandler(
      "/users/{id}",
      [this](const drogon::HttpRequestPtr &,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int userId) {
        deleteUser(userId, std::move(callback));
      },
      {drogon::Delete});

  // PUT /users/{id} -> полностью обновить пользователя.
  drogon::app().registerHandler(
      "/users/{id}",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int userId) {
        updateUser(userId, request, std::move(callback));
      },
      {drogon::Put});

  // GET /users/{id} -> получить пользователя.
  drogon::app().registerHandler(
      "/users/{id}",
      [this](const drogon::HttpRequestPtr &,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int userId) {
        getUser(userId, std::move(callback));
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
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  if (!(*json)["email"].isString() || !(*json)["password"].isString() ||
      !(*json)["name"].isString() ||
      !iot::validation::isValidUserFields(
          (*json)["email"].asString(), (*json)["password"].asString(),
          (*json)["name"].asString())) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "email, password and name are required non-empty strings";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
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

  service_->save(
      (*json)["email"].asString(),
      (*json)["password"].asString(),
      (*json)["name"].asString(),
      telegramChatId,
      [callback](int userId) {
        Json::Value body;
        body["status"] = "ok";
        body["user_id"] = userId;
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k201Created);
        response->addHeader("Location", "/users/" + std::to_string(userId));
        callback(response);
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
      });
}

void UsersController::deleteUser(
    int userId,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  if (userId <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive user ID in the URL";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  service_->deleteById(
      userId,
      [callback]() {
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(drogon::k204NoContent);
        callback(response);
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
      });
}

void UsersController::updateUser(
    int userId, const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Invalid JSON";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  if (userId <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive integer id parameter";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
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

  if (!(*json)["email"].isString() || !(*json)["password"].isString() ||
      !(*json)["name"].isString() ||
      !iot::validation::isValidUserFields(
          (*json)["email"].asString(), (*json)["password"].asString(),
          (*json)["name"].asString())) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "email, password and name are required non-empty strings";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  const std::string email = (*json)["email"].asString();
  const std::string password = (*json)["password"].asString();
  const std::string name = (*json)["name"].asString();

  service_->updateById(
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
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
      });
}

void UsersController::getUser(
    int userId,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  if (userId <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive integer id parameter";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  service_->getById(
      userId,
      [callback](const std::string &responseBody) {
        Json::Value body;
        Json::CharReaderBuilder builder;
        std::string errs;
        std::istringstream iss(responseBody);
        if (Json::parseFromStream(builder, iss, &body, &errs)) {
          auto response = drogon::HttpResponse::newHttpJsonResponse(body);
          if (body.isObject() && body["status"].isString() &&
              body["status"].asString() == "error") {
            response->setStatusCode(drogon::k404NotFound);
          }
          callback(response);
        } else {
          Json::Value errorBody;
          errorBody["status"] = "error";
          errorBody["message"] = "Failed to parse user data";
          auto response = drogon::HttpResponse::newHttpJsonResponse(errorBody);
          response->setStatusCode(drogon::k500InternalServerError);
          callback(response);
        }
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
      });
}
