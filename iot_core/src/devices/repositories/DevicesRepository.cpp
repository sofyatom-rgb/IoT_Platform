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

void DevicesRepository::getByOwnerId(
    int ownerId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  LOG_INFO << "[iot_core][db] devices.getByOwnerId: started";
  db_->execSqlAsync(
      "SELECT id, owner_id, name, type FROM devices WHERE owner_id = $1",
      [success](const drogon::orm::Result &r) {
        LOG_INFO << "[iot_core][db] devices.getByOwnerId: completed";
        Json::Value array(Json::arrayValue);
        for (const auto &row : r) {
          Json::Value item;
          item["id"] = row["id"].as<int>();
          item["owner_id"] = row["owner_id"].as<int>();
          item["name"] = row["name"].as<std::string>();
          item["type"] = row["type"].as<std::string>();
          array.append(item);
        }
        success(Json::FastWriter().write(array));
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] devices.getByOwnerId failed";
        error(e.base().what());
      },
      ownerId);
}

void DevicesRepository::checkPermission(int userId, const std::string &method,
                                         std::function<void(bool)> success,
                                         std::function<void(const std::string &)> error) const {
  LOG_INFO << "[iot_core][db] devices.checkPermission: started";
  // Репозиторий тоже может быть уничтожен до завершения асинхронного запроса.
  auto db = db_;
  db->execSqlAsync(
      "SELECT us.status FROM user_status us WHERE us.user_id = $1",
      [db, method, success, error](const drogon::orm::Result &r) {
        LOG_INFO << "[iot_core][db] devices.checkPermission: status loaded";
        if (r.empty()) {
          success(false);
          return;
        }
        const std::string status = r[0]["status"].as<std::string>();

        std::string column;
        if (method == "GET")
          column = "can_get";
        else if (method == "POST")
          column = "can_post";
        else if (method == "PUT")
          column = "can_put";
        else if (method == "DELETE")
          column = "can_delete";
        else {
          success(false);
          return;
        }

        db->execSqlAsync(
            "SELECT " + column + " FROM permissions WHERE status = $1",
            [success](const drogon::orm::Result &r) {
              LOG_INFO << "[iot_core][db] devices.checkPermission: flag loaded";
              if (r.empty()) {
                success(false);
                return;
              }
              success(r[0][0].as<bool>());
            },
            [error](const drogon::orm::DrogonDbException &e) {
              LOG_ERROR << "[iot_core][db] devices.checkPermission flag query failed";
              error(e.base().what());
            },
            status);
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] devices.checkPermission status query failed";
        error(e.base().what());
      },
      userId);
}

void DevicesRepository::checkDeviceOwnership(int deviceId, int userId,
                                              std::function<void(bool)> success,
                                              std::function<void(const std::string &)> error) const {
  LOG_INFO << "[iot_core][db] devices.checkDeviceOwnership: started";
  db_->execSqlAsync(
      "SELECT owner_id FROM devices WHERE id = $1",
      [success, userId](const drogon::orm::Result &r) {
        LOG_INFO << "[iot_core][db] devices.checkDeviceOwnership: completed";
        if (r.empty()) {
          success(false);
          return;
        }
        success(r[0]["owner_id"].as<int>() == userId);
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[iot_core][db] devices.checkDeviceOwnership failed";
        error(e.base().what());
      },
      deviceId);
}