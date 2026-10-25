#include "devices/repositories/DevicesRepository.h"

#include <utility>

DevicesRepository::DevicesRepository(drogon::orm::DbClientPtr db)
    : db_(std::move(db)) {}

void DevicesRepository::save(int ownerId,
                             const std::string &name,
                             const std::string &type,
                             std::function<void(int)> success,
                             std::function<void(const std::string &)> error) const {
  db_->execSqlAsync(
      "INSERT INTO devices (owner_id, name, type) VALUES ($1, $2, $3) RETURNING id",
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
      ownerId, name, type);
}

void DevicesRepository::deleteById(int deviceId,
                                   std::function<void()> success,
                                   std::function<void(const std::string &)> error) const {
  db_->execSqlAsync(
      "DELETE FROM devices WHERE id = $1",
      [success](const drogon::orm::Result &r) {
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        error(e.base().what());
      },
      deviceId);
}

void DevicesRepository::getNotificationInfo(
    int deviceId,
    std::function<void(int, const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  db_->execSqlAsync(
      "SELECT owner_id, name FROM devices WHERE id = $1",
      [success, error](const drogon::orm::Result &result) {
        if (result.empty()) {
          error("Device not found");
          return;
        }
        success(result[0]["owner_id"].as<int>(),
                result[0]["name"].as<std::string>());
      },
      [error](const drogon::orm::DrogonDbException &e) {
        error(e.base().what());
      },
      deviceId);
}