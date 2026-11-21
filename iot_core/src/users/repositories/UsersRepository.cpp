#include "users/repositories/UsersRepository.h"

#include <drogon/drogon.h>

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
  LOG_INFO << "[iot_core][db] users.save: started";
  db_->execSqlAsync(
      "INSERT INTO users (email, password_hash, salt, name, telegram_chat_id) "
      "VALUES ($1, $2, $3, $4, NULLIF($5, '')) RETURNING id",
      [success](const drogon::orm::Result &r) {
        LOG_INFO << "[iot_core][db] users.save: completed";
        if (!r.empty()) {
          success(r[0]["id"].as<int>());
        } else {
          success(-1);
        }
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] users.save failed";
        error(e.base().what());
      },
      email, hash, salt, name, telegramChatId);
}

void UsersRepository::deleteById(int userId,
                                 std::function<void()> success,
                                 std::function<void(const std::string &)> error) const {
  LOG_INFO << "[iot_core][db] users.deleteById: started";
  db_->execSqlAsync(
      "DELETE FROM users WHERE id = $1",
      [success](const drogon::orm::Result &r) {
        LOG_INFO << "[iot_core][db] users.deleteById: completed";
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] users.deleteById failed";
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
  LOG_INFO << "[iot_core][db] users.updateById: started";
  db_->execSqlAsync(
      "UPDATE users SET email = $1, password_hash = $2, salt = $3, "
      "name = $4, telegram_chat_id = NULLIF($5, '') WHERE id = $6",
      [success](const drogon::orm::Result &) {
        LOG_INFO << "[iot_core][db] users.updateById: completed";
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] users.updateById failed";
        error(e.base().what());
      },
      email, hash, salt, name, telegramChatId, userId);
}

void UsersRepository::getById(
    int userId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  LOG_INFO << "[iot_core][db] users.getById: started";
  db_->execSqlAsync(
      "SELECT id, email, name, telegram_chat_id FROM users WHERE id = $1",
      [success, error](const drogon::orm::Result &r) {
        LOG_INFO << "[iot_core][db] users.getById: completed";
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
        LOG_ERROR << "[iot_core][db] users.getById failed";
        error(e.base().what());
      },
      userId);
}

void UsersRepository::getTelegramChatId(
    int userId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  LOG_INFO << "[iot_core][db] users.getTelegramChatId: started";
  db_->execSqlAsync(
      "SELECT telegram_chat_id FROM users WHERE id = $1",
      [success, error](const drogon::orm::Result &result) {
        LOG_INFO << "[iot_core][db] users.getTelegramChatId: completed";
        if (result.empty()) {
          error("User not found");
          return;
        }
        const auto value = result[0]["telegram_chat_id"];
        success(value.isNull() ? std::string{} : value.as<std::string>());
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] users.getTelegramChatId failed";
        error(e.base().what());
      },
      userId);
}