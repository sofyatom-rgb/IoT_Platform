#include "telemetry/repositories/TelemetryRepository.h"

#include <drogon/drogon.h>

#include <utility>

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
      "SELECT id, device_id, temperature, time FROM telemetry WHERE device_id = $1 ORDER BY id DESC LIMIT 10",
      [success](const drogon::orm::Result &r) {
        LOG_INFO << "[iot_core][db] telemetry.getByDeviceId: completed";
        Json::Value array(Json::arrayValue);
        for (const auto &row : r) {
          Json::Value item;
          item["id"] = row["id"].as<int>();
          item["device_id"] = row["device_id"].as<int>();
          item["temperature"] = row["temperature"].as<double>();
          item["time"] = row["time"].as<std::string>();
          array.append(item);
        }
        success(Json::FastWriter().write(array));
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] telemetry.getByDeviceId failed";
        error(e.base().what());
      },
      deviceId);
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
