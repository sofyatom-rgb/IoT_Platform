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