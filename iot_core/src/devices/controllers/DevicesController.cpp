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

  // DELETE /devices?id=X -> удалить устройство и его telemetry.
  drogon::app().registerHandler(
      "/devices",
      [this](const drogon::HttpRequestPtr &request,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        deleteDevice(request, std::move(callback));
      },
      {drogon::Delete});
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

  DevicesService service(drogon::app().getDbClient());
  service.save(
      (*json)["owner_id"].asInt(),
      (*json)["name"].asString(),
      (*json)["type"].asString(),
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
  std::string deviceIdStr = request->getParameter("id");
  if (deviceIdStr.empty()) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Missing id parameter";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
    return;
  }

  int deviceId = std::stoi(deviceIdStr);

  DevicesService service(drogon::app().getDbClient());
  service.deleteById(
      deviceId,
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