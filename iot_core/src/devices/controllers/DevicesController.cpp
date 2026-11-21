#include "devices/controllers/DevicesController.h"

#include <string>
#include <utility>

void DevicesController::registerRoutes() {
  // POST /devices -> создать устройство.
  drogon::app().registerHandler(
      "/devices",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        create(request, std::move(callback));
      },
      {drogon::Post});

  // DELETE /devices with JSON {"device_id": X} -> удалить устройство.
  drogon::app().registerHandler(
      "/devices",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        deleteDevice(request, std::move(callback));
      },
      {drogon::Delete});

  // PUT /devices?id=X -> обновить устройство.
  drogon::app().registerHandler(
      "/devices",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        updateDevice(request, std::move(callback));
      },
      {drogon::Put});

  // GET /devices?id=X -> получить устройство.
  drogon::app().registerHandler(
      "/devices",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        getDevice(request, std::move(callback));
      },
      {drogon::Get});
}

void DevicesController::create(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();

  if (!json) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Invalid JSON";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  if (!(*json)["user_id"].isInt() || (*json)["user_id"].asInt() <= 0 ||
      !(*json)["owner_id"].isInt() || (*json)["owner_id"].asInt() <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "user_id and owner_id must be positive integers";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  int userId = (*json)["user_id"].asInt();
  int ownerId = (*json)["owner_id"].asInt();
  std::string name = (*json)["name"].asString();
  std::string type = (*json)["type"].asString();

  DevicesService service(drogon::app().getDbClient());
  service.save(
      userId, ownerId, name, type,
      [callback](int deviceId) {
        Json::Value body;
        body["status"] = "ok";
        body["device_id"] = deviceId;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}

void DevicesController::deleteDevice(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json || !(*json)["user_id"].isInt() || (*json)["user_id"].asInt() <= 0 ||
      !(*json)["device_id"].isInt() || (*json)["device_id"].asInt() <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "user_id and device_id must be positive integers in JSON body";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  const int userId = (*json)["user_id"].asInt();
  const int deviceId = (*json)["device_id"].asInt();

  DevicesService service(drogon::app().getDbClient());
  service.deleteById(
      userId, deviceId,
      [callback]() {
        Json::Value body;
        body["status"] = "ok";
        body["message"] = "Device deleted";
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}
void DevicesController::updateDevice(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json || !(*json)["user_id"].isInt() || (*json)["user_id"].asInt() <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "user_id must be a positive integer in JSON body";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  std::string deviceIdStr = request->getParameter("id");
  if (deviceIdStr.empty()) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Missing id parameter";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  int userId = (*json)["user_id"].asInt();
  int deviceId = std::stoi(deviceIdStr);
  int ownerId = (*json)["owner_id"].asInt();
  std::string name = (*json)["name"].asString();
  std::string type = (*json)["type"].asString();

  DevicesService service(drogon::app().getDbClient());
  service.updateById(
      userId, deviceId, ownerId, name, type,
      [callback]() {
        Json::Value body;
        body["status"] = "ok";
        body["message"] = "Device updated";
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}

void DevicesController::getDevice(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  std::string deviceIdStr = request->getParameter("id");
  if (deviceIdStr.empty()) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Missing id parameter";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  std::string userIdStr = request->getParameter("user_id");
  if (userIdStr.empty()) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Missing user_id parameter";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  int deviceId = std::stoi(deviceIdStr);
  int userId = std::stoi(userIdStr);

  DevicesService service(drogon::app().getDbClient());
  service.getById(
      userId, deviceId,
      [callback](const std::string &responseBody) {
        Json::Value body;
        Json::CharReaderBuilder builder;
        std::string errs;
        std::istringstream iss(responseBody);
        if (Json::parseFromStream(builder, iss, &body, &errs)) {
          callback(drogon::HttpResponse::newHttpJsonResponse(body));
        } else {
          Json::Value errorBody;
          errorBody["status"] = "error";
          errorBody["message"] = "Failed to parse device data";
          callback(drogon::HttpResponse::newHttpJsonResponse(errorBody));
        }
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}