#include "devices/repositories/DevicesRepository.h"

#include <drogon/drogon.h>

#include <utility>

DevicesRepository::DevicesRepository(drogon::orm::DbClientPtr db)
    : db_(std::move(db)) {}

void DevicesRepository::save(int ownerId,
                             const std::string &name,
                             const std::string &type,
                             std::function<void(int)> success,
                             std::function<void(const std::string &)> error) const {
  LOG_INFO << "[iot_core][db] devices.save: started";
  db_->execSqlAsync(
      "INSERT INTO devices (owner_id, name, type) VALUES ($1, $2, $3) RETURNING id",
      [success](const drogon::orm::Result &r) {
        LOG_INFO << "[iot_core][db] devices.save: completed";
        if (!r.empty()) {
          success(r[0]["id"].as<int>());
        } else {
          success(-1);
        }
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] devices.save failed";
        error(e.base().what());
      },
      ownerId, name, type);
}

void DevicesRepository::deleteById(int deviceId,
                                   std::function<void()> success,
                                   std::function<void(const std::string &)> error) const {
  LOG_INFO << "[iot_core][db] devices.deleteById: started";
  db_->execSqlAsync(
      "DELETE FROM devices WHERE id = $1",
      [success](const drogon::orm::Result &r) {
        LOG_INFO << "[iot_core][db] devices.deleteById: completed";
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] devices.deleteById failed";
        error(e.base().what());
      },
      deviceId);
}

void DevicesRepository::updateById(int deviceId,
                                   int ownerId,
                                   const std::string &name,
                                   const std::string &type,
                                   std::function<void()> success,
                                   std::function<void(const std::string &)> error) const {
  LOG_INFO << "[iot_core][db] devices.updateById: started";
  db_->execSqlAsync(
      "UPDATE devices SET owner_id = $1, name = $2, type = $3 WHERE id = $4",
      [success](const drogon::orm::Result &) {
        LOG_INFO << "[iot_core][db] devices.updateById: completed";
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] devices.updateById failed";
        error(e.base().what());
      },
      ownerId, name, type, deviceId);
}

void DevicesRepository::getById(
    int deviceId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  LOG_INFO << "[iot_core][db] devices.getById: started";
  db_->execSqlAsync(
      "SELECT id, owner_id, name, type FROM devices WHERE id = $1",
      [success, error](const drogon::orm::Result &r) {
        LOG_INFO << "[iot_core][db] devices.getById: completed";
        if (r.empty()) {
          Json::Value body;
          body["status"] = "error";
          body["message"] = "Device not found";
          success(Json::FastWriter().write(body));
          return;
        }
        Json::Value item;
        item["id"] = r[0]["id"].as<int>();
        item["owner_id"] = r[0]["owner_id"].as<int>();
        item["name"] = r[0]["name"].as<std::string>();
        item["type"] = r[0]["type"].as<std::string>();
        success(Json::FastWriter().write(item));
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] devices.getById failed";
        error(e.base().what());
      },
      deviceId);
}

void DevicesRepository::getNotificationInfo(
    int deviceId,
    std::function<void(int, const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  LOG_INFO << "[iot_core][db] devices.getNotificationInfo: started";
  db_->execSqlAsync(
      "SELECT owner_id, name FROM devices WHERE id = $1",
      [success, error](const drogon::orm::Result &result) {
        LOG_INFO << "[iot_core][db] devices.getNotificationInfo: completed";
        if (result.empty()) {
          error("Device not found");
          return;
        }
        success(result[0]["owner_id"].as<int>(),
                result[0]["name"].as<std::string>());
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] devices.getNotificationInfo failed";
        error(e.base().what());
      },
      deviceId);
}