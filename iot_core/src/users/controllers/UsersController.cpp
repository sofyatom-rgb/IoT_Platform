#include "common/db/DbClientProvider.h"
#include "users/controllers/UsersController.h"

#include <cctype>
#include <sstream>
#include <string>
#include <utility>

namespace {
using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

// Отправляет JSON-ошибку с нужным HTTP-статусом.
void replyError(const Callback &cb, drogon::HttpStatusCode code,
                const std::string &msg) {
  Json::Value b;
  b["status"] = "error";
  b["message"] = msg;
  auto r = drogon::HttpResponse::newHttpJsonResponse(b);
  r->setStatusCode(code);
  cb(r);
}

// Ошибку БД логируем, клиенту отдаём общий текст.
void replyDbError(const Callback &cb, const std::string &error) {
  LOG_ERROR << "DB error: " << error;
  replyError(cb, drogon::k500InternalServerError, "internal error");
}

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

// Разбирает id из query-параметра; false, если это не число.
bool parseId(const std::string &s, int &out) {
  try {
    std::size_t pos = 0;
    out = std::stoi(s, &pos);
    return pos == s.size();
  } catch (...) {
    return false;
  }
}
}  // namespace

void UsersController::registerRoutes() {
  // id в URI, но менять можно только свой профиль.
  const auto isSelf = [](const drogon::HttpRequestPtr &request, int userId) {
    return userId > 0 && request->attributes()->get<int>("user_id") == userId;
  };
  const auto forbidden = [](const Callback &cb) {
    replyError(cb, drogon::k403Forbidden, "forbidden");
  };

  drogon::app().registerHandler(
      "/users/{id}",
      [this, isSelf, forbidden](const drogon::HttpRequestPtr &request,
                                Callback &&cb, int userId) {
        if (!isSelf(request, userId)) {
          forbidden(cb);
          return;
        }
        request->setParameter("id", std::to_string(userId));
        deleteUser(request, std::move(cb));
      },
      {drogon::Delete, "AuthFilter"});

  drogon::app().registerHandler(
      "/users/{id}",
      [this, isSelf, forbidden](const drogon::HttpRequestPtr &request,
                                Callback &&cb, int userId) {
        if (!isSelf(request, userId)) {
          forbidden(cb);
          return;
        }
        request->setParameter("id", std::to_string(userId));
        updateUser(request, std::move(cb));
      },
      {drogon::Put, "AuthFilter"});

  drogon::app().registerHandler(
      "/users/{id}",
      [this, isSelf, forbidden](const drogon::HttpRequestPtr &request,
                                Callback &&cb, int userId) {
        if (!isSelf(request, userId)) {
          forbidden(cb);
          return;
        }
        request->setParameter("id", std::to_string(userId));
        getUser(request, std::move(cb));
      },
      {drogon::Get, "AuthFilter"});

  // POST /users -> создать пользователя (без авторизации: это регистрация).
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
    replyError(callback, drogon::k400BadRequest, "Invalid JSON");
    return;
  }

  if (!(*json)["email"].isString() || (*json)["email"].asString().empty() ||
      !(*json)["password"].isString() ||
      (*json)["password"].asString().size() < 8) {
    replyError(callback, drogon::k400BadRequest,
               "email and password (min 8 chars) are required");
    return;
  }

  std::string telegramChatId;
  if (!getTelegramChatId(*json, telegramChatId)) {
    replyError(callback, drogon::k400BadRequest,
               "telegram_chat_id must be an integer or numeric string");
    return;
  }

  UsersService service(DbClientProvider::get());
  service.save(
      (*json)["email"].asString(),
      (*json)["password"].asString(),
      (*json)["name"].asString(),
      telegramChatId,
      [callback](int userId) {
        Json::Value body;
        body["status"] = "ok";
        body["user_id"] = userId;
        auto r = drogon::HttpResponse::newHttpJsonResponse(body);
        r->setStatusCode(drogon::k201Created);
        r->addHeader("Location", "/users/" + std::to_string(userId));
        callback(r);
      },
      [callback](const std::string &error) {
        // Повторный email нарушает UNIQUE -> 409
        if (error.find("unique") != std::string::npos ||
            error.find("duplicate") != std::string::npos) {
          replyError(callback, drogon::k409Conflict,
                     "user with this email or telegram_chat_id already exists");
          return;
        }
        replyDbError(callback, error);
      });
}

void UsersController::deleteUser(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  // id берём из токена: удалить можно только себя.
  const int userId = request->attributes()->get<int>("user_id");

  UsersService service(DbClientProvider::get());
  service.deleteById(
      userId,
      [callback]() {
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(drogon::k204NoContent);
        callback(response);
      },
      [callback](const std::string &error) { replyDbError(callback, error); });
}

void UsersController::updateUser(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json) {
    replyError(callback, drogon::k400BadRequest, "Invalid JSON");
    return;
  }

  // Пароль передаётся только при смене; для обычного обновления профиля он не нужен.
  std::string password;
  if ((*json)["password"].isString() && !(*json)["password"].asString().empty()) {
    password = (*json)["password"].asString();
    if (password.size() < 8) {
      replyError(callback, drogon::k400BadRequest,
                 "password must be at least 8 characters");
      return;
    }
  }

  const std::string userIdStr = request->getParameter("id");
  if (userIdStr.empty()) {
    replyError(callback, drogon::k400BadRequest, "Missing id parameter");
    return;
  }

  int userId = 0;
  if (!parseId(userIdStr, userId)) {
    replyError(callback, drogon::k400BadRequest, "id must be an integer");
    return;
  }

  const int me = request->attributes()->get<int>("user_id");
  if (userId != me) {
    replyError(callback, drogon::k403Forbidden, "forbidden");
    return;
  }

  std::string email = (*json)["email"].asString();
  std::string name = (*json)["name"].asString();
  std::string telegramChatId;
  if (!getTelegramChatId(*json, telegramChatId)) {
    replyError(callback, drogon::k400BadRequest,
               "telegram_chat_id must be an integer or numeric string");
    return;
  }

  auto success = [callback]() {
    Json::Value body;
    body["status"] = "ok";
    body["message"] = "User updated";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
  };
  auto failure = [callback](const std::string &error) {
    if (error.find("unique") != std::string::npos ||
        error.find("duplicate") != std::string::npos) {
      replyError(callback, drogon::k409Conflict,
                 "user with this email or telegram_chat_id already exists");
      return;
    }
    replyDbError(callback, error);
  };

  UsersService service(DbClientProvider::get());
  if (password.empty()) {
    service.updateProfileById(userId, email, name, telegramChatId, success, failure);
  } else {
    service.updateById(userId, email, password, name, telegramChatId, success, failure);
  }
}

void UsersController::getUser(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const std::string userIdStr = request->getParameter("id");
  if (userIdStr.empty()) {
    replyError(callback, drogon::k400BadRequest, "Missing id parameter");
    return;
  }

  int userId = 0;
  if (!parseId(userIdStr, userId)) {
    replyError(callback, drogon::k400BadRequest, "id must be an integer");
    return;
  }

  const int me = request->attributes()->get<int>("user_id");
  if (userId != me) {
    replyError(callback, drogon::k403Forbidden, "forbidden");
    return;
  }

  UsersService service(DbClientProvider::get());
  service.getById(
      userId,
      [callback](const std::string &responseBody) {
        Json::Value body;
        Json::CharReaderBuilder builder;
        std::string errs;
        std::istringstream iss(responseBody);
        if (!Json::parseFromStream(builder, iss, &body, &errs)) {
          replyError(callback, drogon::k500InternalServerError,
                     "Failed to parse user data");
          return;
        }
        // Репозиторий сообщает «не найден» через status=error
        if (body.isMember("status") && body["status"] == "error") {
          replyError(callback, drogon::k404NotFound, "User not found");
          return;
        }
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      [callback](const std::string &error) { replyDbError(callback, error); });
}