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
  LOG_INFO << "[iot_core][db] telemetry.save: started";
  db_->execSqlAsync(
      "INSERT INTO telemetry (device_id, temperature) VALUES ($1, $2)",
      [success](const drogon::orm::Result &) {
        LOG_INFO << "[iot_core][db] telemetry.save: completed";
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] telemetry.save failed";
        error(e.base().what());
      },
      deviceId, temperature);
}

void TelemetryRepository::getByDeviceId(
    int deviceId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  LOG_INFO << "[iot_core][db] telemetry.getByDeviceId: started";
  db_->execSqlAsync(
      "SELECT id, device_id, temperature, time FROM telemetry WHERE device_id = $1 ORDER BY id DESC",
      [success](const drogon::orm::Result &r) {
        LOG_INFO << "[iot_core][db] telemetry.getByDeviceId: completed";
        success(serializeTelemetry(r));
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] telemetry.getByDeviceId failed";
        error(e.base().what());
      },
      deviceId);
}

void TelemetryRepository::queryByDeviceId(
    int deviceId, const TelemetryQuery &query,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  if (query.afterId) {
    LOG_INFO << "[iot_core][db] telemetry.queryByDeviceId (after): started";
    db_->execSqlAsync(
        "SELECT id, device_id, temperature, time FROM telemetry "
        "WHERE device_id = $1 AND id > $2 ORDER BY id ASC LIMIT 5000",
        [success](const drogon::orm::Result &rows) {
          LOG_INFO << "[iot_core][db] telemetry.queryByDeviceId (after): completed";
          success(serializeTelemetry(rows));
        },
        [error](const drogon::orm::DrogonDbException &e) {
          LOG_ERROR << "[iot_core][db] telemetry.queryByDeviceId (after) failed";
          error(e.base().what());
        },
        deviceId, *query.afterId);
    return;
  }

  if (query.allTime) {
    LOG_INFO << "[iot_core][db] telemetry.queryByDeviceId (all): started";
    db_->execSqlAsync(
        "SELECT id, device_id, temperature, time FROM telemetry "
        "WHERE device_id = $1 ORDER BY time DESC, id DESC",
        [success](const drogon::orm::Result &rows) {
          LOG_INFO << "[iot_core][db] telemetry.queryByDeviceId (all): completed";
          success(serializeTelemetry(rows));
        },
        [error](const drogon::orm::DrogonDbException &e) {
          LOG_ERROR << "[iot_core][db] telemetry.queryByDeviceId (all) failed";
          error(e.base().what());
        },
        deviceId);
    return;
  }

  if (query.from && query.to) {
    LOG_INFO << "[iot_core][db] telemetry.queryByDeviceId (range): started";
    db_->execSqlAsync(
        "SELECT id, device_id, temperature, time FROM telemetry "
        "WHERE device_id = $1 "
        "AND time >= ($2::timestamptz AT TIME ZONE 'UTC') "
        "AND time <= ($3::timestamptz AT TIME ZONE 'UTC') "
        "ORDER BY time DESC, id DESC",
        [success](const drogon::orm::Result &rows) {
          LOG_INFO << "[iot_core][db] telemetry.queryByDeviceId (range): completed";
          success(serializeTelemetry(rows));
        },
        [error](const drogon::orm::DrogonDbException &e) {
          LOG_ERROR << "[iot_core][db] telemetry.queryByDeviceId (range) failed";
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
  LOG_INFO << "[iot_core][db] telemetry.deleteByDeviceId: started";
  db_->execSqlAsync(
      "DELETE FROM telemetry WHERE device_id = $1",
      [success](const drogon::orm::Result &r) {
        LOG_INFO << "[iot_core][db] telemetry.deleteByDeviceId: completed";
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] telemetry.deleteByDeviceId failed";
        error(e.base().what());
      },
      deviceId);
}
