#include "common/db/DbClientProvider.h"
#include "devices/controllers/DevicesController.h"

#include <sstream>
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
      {drogon::Post, "AuthFilter"});

  // DELETE /devices with JSON {"device_id": X} -> удалить устройство.
  drogon::app().registerHandler(
      "/devices",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        deleteDevice(request, std::move(callback));
      },
      {drogon::Delete, "AuthFilter"});

  // PUT /devices?id=X -> обновить устройство.
  drogon::app().registerHandler(
      "/devices",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        updateDevice(request, std::move(callback));
      },
      {drogon::Put, "AuthFilter"});

  // GET /devices?id=X -> получить устройство.
  drogon::app().registerHandler(
      "/devices",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        if (request->getParameter("id").empty()) {
          listDevices(request, std::move(callback));
        } else {
          getDevice(request, std::move(callback));
        }
      },
      {drogon::Get, "AuthFilter"});

  // REST-варианты: id устройства передаётся в URI.
  drogon::app().registerHandler(
      "/devices/{id}",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int deviceId) {
        request->setParameter("device_id", std::to_string(deviceId));
        deleteDevice(request, std::move(callback));
      },
      {drogon::Delete, "AuthFilter"});

  drogon::app().registerHandler(
      "/devices/{id}",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int deviceId) {
        request->setParameter("id", std::to_string(deviceId));
        updateDevice(request, std::move(callback));
      },
      {drogon::Put, "AuthFilter"});

  drogon::app().registerHandler(
      "/devices/{id}",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
             int deviceId) {
        request->setParameter("id", std::to_string(deviceId));
        getDevice(request, std::move(callback));
      },
      {drogon::Get, "AuthFilter"});
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

  const int userId = request->attributes()->get<int>("user_id");
  if (userId <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Unauthorized";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  // Владелец устройства определяется JWT, а не полем запроса.
  const int ownerId = userId;
  std::string name = (*json)["name"].asString();
  std::string type = (*json)["type"].asString();

  DevicesService service(DbClientProvider::get());
  service.save(
      userId, ownerId, name, type,
      [callback](int deviceId) {
        Json::Value body;
        body["status"] = "ok";
        body["device_id"] = deviceId;
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k201Created);
        response->addHeader("Location", "/devices/" + std::to_string(deviceId));
        callback(response);
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
  const int userId = request->attributes()->get<int>("user_id");
  if (userId <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Unauthorized";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  // REST передаёт id в URI, старый вариант — в JSON.
  int deviceId = 0;
  const std::string idParam = request->getParameter("device_id");
  if (!idParam.empty()) {
    try {
      deviceId = std::stoi(idParam);
    } catch (...) {
      deviceId = 0;
    }
  } else if (json && (*json)["device_id"].isInt()) {
    deviceId = (*json)["device_id"].asInt();
  }
  if (deviceId <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "device_id must be a positive integer";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  DevicesService service(DbClientProvider::get());
  service.deleteById(
      userId, deviceId,
      [callback]() {
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(drogon::k204NoContent);
        callback(response);
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
  const int userId = request->attributes()->get<int>("user_id");
  if (userId <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Unauthorized";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  if (!json) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Invalid JSON";
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

  int deviceId = std::stoi(deviceIdStr);
  int ownerId = userId;
  std::string name = (*json)["name"].asString();
  std::string type = (*json)["type"].asString();

  DevicesService service(DbClientProvider::get());
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
  const int userId = request->attributes()->get<int>("user_id");
  if (userId <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Unauthorized";
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

  int deviceId = std::stoi(deviceIdStr);

  DevicesService service(DbClientProvider::get());
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
void DevicesController::listDevices(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const int userId = request->attributes()->get<int>("user_id");
  if (userId <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Unauthorized";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  DevicesService service(DbClientProvider::get());
  service.listByOwner(
      userId,
      [callback](const std::string &responseBody) {
        Json::Value body;
        Json::CharReaderBuilder builder;
        std::string errors;
        std::istringstream input(responseBody);
        if (!Json::parseFromStream(builder, input, &body, &errors)) {
          Json::Value errorBody;
          errorBody["status"] = "error";
          errorBody["message"] = "Failed to parse device list";
          callback(drogon::HttpResponse::newHttpJsonResponse(errorBody));
          return;
        }
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      });
}
