#include "telemetry/repositories/TelemetryRepository.h"

#include <drogon/drogon.h>

#include <utility>

namespace {
std::string serializeTelemetry(const drogon::orm::Result &rows) {
  Json::Value array(Json::arrayValue);
  for (const auto &row : rows) {
    Json::Value item;
    item["id"] = row["id"].as<int>();
    item["device_id"] = row["device_id"].as<int>();
    item["temperature"] = row["temperature"].as<double>();
    item["time"] = row["time"].as<std::string>();
    array.append(item);
  }
  return Json::FastWriter().write(array);
}
}  // namespace

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

void TelemetryRepository::getByDeviceId(
    int deviceId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  db_->execSqlAsync(
      "SELECT id, device_id, temperature, time FROM telemetry WHERE device_id = $1 ORDER BY id DESC",
      [success](const drogon::orm::Result &r) { success(serializeTelemetry(r)); },
      [error](const drogon::orm::DrogonDbException &e) {
        error(e.base().what());
      },
      deviceId);
}

void TelemetryRepository::queryByDeviceId(
    int deviceId, const TelemetryQuery &query,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  if (query.afterId) {
    db_->execSqlAsync(
        "SELECT id, device_id, temperature, time FROM telemetry "
        "WHERE device_id = $1 AND id > $2 ORDER BY id ASC LIMIT 5000",
        [success](const drogon::orm::Result &rows) {
          success(serializeTelemetry(rows));
        },
        [error](const drogon::orm::DrogonDbException &e) {
          error(e.base().what());
        },
        deviceId, *query.afterId);
    return;
  }

  if (query.allTime) {
    db_->execSqlAsync(
        "SELECT id, device_id, temperature, time FROM telemetry "
        "WHERE device_id = $1 ORDER BY time DESC, id DESC",
        [success](const drogon::orm::Result &rows) {
          success(serializeTelemetry(rows));
        },
        [error](const drogon::orm::DrogonDbException &e) {
          error(e.base().what());
        },
        deviceId);
    return;
  }

  if (query.from && query.to) {
    db_->execSqlAsync(
        "SELECT id, device_id, temperature, time FROM telemetry "
        "WHERE device_id = $1 "
        "AND time >= ($2::timestamptz AT TIME ZONE 'UTC') "
        "AND time <= ($3::timestamptz AT TIME ZONE 'UTC') "
        "ORDER BY time DESC, id DESC",
        [success](const drogon::orm::Result &rows) {
          success(serializeTelemetry(rows));
        },
        [error](const drogon::orm::DrogonDbException &e) {
          error(e.base().what());
        },
        deviceId, *query.from, *query.to);
    return;
  }

  getByDeviceId(deviceId, std::move(success), std::move(error));
}

void TelemetryRepository::deleteByDeviceId(
    int deviceId,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  db_->execSqlAsync(
      "DELETE FROM telemetry WHERE device_id = $1",
      [success](const drogon::orm::Result &r) {
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        error(e.base().what());
      },
      deviceId);
}
