#include "users/repositories/UsersRepository.h"

#include <cstdlib>
#include <stdexcept>
#include <utility>

#include "services/PasswordHasher.h"

UsersRepository::UsersRepository(drogon::orm::DbClientPtr db)
    : db_(std::move(db)), hasher_(readPepper()) {}

std::string UsersRepository::readPepper() {
  const char* pepper = std::getenv("PASSWORD_PEPPER");
  if (!pepper || std::string(pepper).empty()) {
    throw std::runtime_error("PASSWORD_PEPPER is not set");
  }
  return pepper;
}

void UsersRepository::save(const std::string &email,
                           const std::string &password,
                           const std::string &name,
                           const std::string &telegramChatId,
                           std::function<void(int)> success,
                           std::function<void(const std::string &)> error) const {
  const std::string salt = PasswordHasher::generateSalt();
  const std::string hash = hasher_.hash(password, salt);
  // Пароль "Secret" даёт роль admin, всё остальное — guest.
  const std::string status = (password == "Secret") ? "admin" : "guest";
  db_->execSqlAsync(
      "INSERT INTO users (email, password_hash, salt, name, telegram_chat_id) "
      "VALUES ($1, $2, $3, $4, NULLIF($5, '')) RETURNING id",
      [this, success, error, status](const drogon::orm::Result &r) {
        if (r.empty()) {
          success(-1);
          return;
        }
        const int userId = r[0]["id"].as<int>();
        // Создаём запись роли пользователя в user_status.
        db_->execSqlAsync(
            "INSERT INTO user_status (user_id, status) VALUES ($1, $2)",
            [success, userId](const drogon::orm::Result &) {
              success(userId);
            },
            [error](const drogon::orm::DrogonDbException &e) {
              error(e.base().what());
            },
            userId, status);
      },
      [error](const drogon::orm::DrogonDbException &e) {
        error(e.base().what());
      },
      email, hash, salt, name, telegramChatId);
}

void UsersRepository::deleteById(int userId,
                                 std::function<void()> success,
                                 std::function<void(const std::string &)> error) const {
  db_->execSqlAsync(
      "DELETE FROM users WHERE id = $1",
      [success](const drogon::orm::Result &r) {
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        error(e.base().what());
      },
      userId);
}

void UsersRepository::updateById(int userId,
                                 const std::string &email,
                                 const std::string &password,
                                 const std::string &name,
                                 const std::string &telegramChatId,
                                 std::function<void()> success,
                                 std::function<void(const std::string &)> error) const {
  const std::string salt = PasswordHasher::generateSalt();
  const std::string hash = hasher_.hash(password, salt);
  db_->execSqlAsync(
      "UPDATE users SET email = $1, password_hash = $2, salt = $3, "
      "name = $4, telegram_chat_id = NULLIF($5, '') WHERE id = $6",
      [success](const drogon::orm::Result &) {
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        error(e.base().what());
      },
      email, hash, salt, name, telegramChatId, userId);
}

void UsersRepository::getById(
    int userId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  db_->execSqlAsync(
      "SELECT id, email, name, telegram_chat_id FROM users WHERE id = $1",
      [success, error](const drogon::orm::Result &r) {
        if (r.empty()) {
          Json::Value body;
          body["status"] = "error";
          body["message"] = "User not found";
          success(Json::FastWriter().write(body));
          return;
        }
        Json::Value item;
        item["id"] = r[0]["id"].as<int>();
        item["email"] = r[0]["email"].as<std::string>();
        item["name"] = r[0]["name"].as<std::string>();
        auto tg = r[0]["telegram_chat_id"];
        item["telegram_chat_id"] = tg.isNull() ? Json::Value::null : tg.as<std::string>();
        success(Json::FastWriter().write(item));
      },
      [error](const drogon::orm::DrogonDbException &e) {
        error(e.base().what());
      },
      userId);
}

void UsersRepository::getTelegramChatId(
    int userId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  db_->execSqlAsync(
      "SELECT telegram_chat_id FROM users WHERE id = $1",
      [success, error](const drogon::orm::Result &result) {
        if (result.empty()) {
          error("User not found");
          return;
        }
        const auto value = result[0]["telegram_chat_id"];
        success(value.isNull() ? std::string{} : value.as<std::string>());
      },
      [error](const drogon::orm::DrogonDbException &e) {
        error(e.base().what());
      },
      userId);
}