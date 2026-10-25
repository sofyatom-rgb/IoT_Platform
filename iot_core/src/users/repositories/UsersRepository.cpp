#include "users/repositories/UsersRepository.h"

#include <utility>

UsersRepository::UsersRepository(drogon::orm::DbClientPtr db)
    : db_(std::move(db)) {}

void UsersRepository::save(const std::string &email,
                           const std::string &password,
                           const std::string &name,
                           const std::string &telegramChatId,
                           std::function<void(int)> success,
                           std::function<void(const std::string &)> error) const {
  db_->execSqlAsync(
      "INSERT INTO users (email, password, name, telegram_chat_id) "
      "VALUES ($1, $2, $3, NULLIF($4, '')) RETURNING id",
      [success](const drogon::orm::Result &r) {
        if (!r.empty()) {
          success(r[0]["id"].as<int>());
        } else {
          success(-1);
        }
      },
      [error](const drogon::orm::DrogonDbException &e) {
        error(e.base().what());
      },
      email, password, name, telegramChatId);
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
  db_->execSqlAsync(
      "UPDATE users SET email = $1, password = $2, name = $3, telegram_chat_id = NULLIF($4, '') WHERE id = $5",
      [success](const drogon::orm::Result &) {
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        error(e.base().what());
      },
      email, password, name, telegramChatId, userId);
}

void UsersRepository::getById(
    int userId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  db_->execSqlAsync(
      "SELECT id, email, password, name, telegram_chat_id FROM users WHERE id = $1",
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
        item["password"] = r[0]["password"].as<std::string>();
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