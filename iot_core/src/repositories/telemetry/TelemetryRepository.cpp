#include "repositories/telemetry/TelemetryRepository.h"

#include <utility>

TelemetryRepository::TelemetryRepository(drogon::orm::DbClientPtr db)
    : db_(std::move(db)) {}

void TelemetryRepository::save(
    int deviceId,
    double temperature,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  // Записываем показание в PostgreSQL.
  db_->execSqlAsync(
      "INSERT INTO telemetry (device_id, temperature) VALUES ($1, $2)",
      [success](const drogon::orm::Result &) { success(); },
      [error](const drogon::orm::DrogonDbException &e) {
        error(e.base().what());
      },
      deviceId, temperature);
}
