#include "devices/controllers/DevicesController.h"
#include "common/validation/RequestValidation.h"

#include <memory>
#include <stdexcept>

#include <sstream>
#include <string>
#include <utility>

DevicesController::DevicesController(std::shared_ptr<IDevicesService> service)
    : service_(std::move(service)) {
  if (!service_) {
    throw std::invalid_argument("DevicesController requires a devices service");
  }
}

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
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  if (!(*json)["owner_id"].isInt() || !(*json)["name"].isString() ||
      !(*json)["type"].isString()) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "owner_id must be an integer; name and type must be strings";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }
  const int ownerId = (*json)["owner_id"].asInt();
  const std::string name = (*json)["name"].asString();
  const std::string type = (*json)["type"].asString();
  if (!iot::validation::isValidDeviceFields(ownerId, name, type)) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive owner_id and non-empty name and type";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  service_->save(
      ownerId, name, type,
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
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
      });
}

void DevicesController::deleteDevice(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json || !(*json)["device_id"].isInt() ||
      (*json)["device_id"].asInt() <= 0) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive integer device_id in JSON body";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  const int deviceId = (*json)["device_id"].asInt();

  service_->deleteById(
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
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
      });
}
void DevicesController::updateDevice(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto json = request->getJsonObject();
  if (!json) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Invalid JSON";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  const auto deviceId = iot::validation::parsePositiveId(request->getParameter("id"));
  if (!(*json)["owner_id"].isInt() || !(*json)["name"].isString() ||
      !(*json)["type"].isString()) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "owner_id must be an integer; name and type must be strings";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }
  const int ownerId = (*json)["owner_id"].asInt();
  const std::string name = (*json)["name"].asString();
  const std::string type = (*json)["type"].asString();
  if (!deviceId || !iot::validation::isValidDeviceFields(ownerId, name, type)) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive id, positive owner_id and non-empty name and type";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  service_->updateById(
      *deviceId, ownerId, name, type,
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
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
      });
}

void DevicesController::getDevice(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto deviceId = iot::validation::parsePositiveId(request->getParameter("id"));
  if (!deviceId) {
    Json::Value body;
    body["status"] = "error";
    body["message"] = "Expected a positive integer id parameter";
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  service_->getById(
      *deviceId,
      [callback](const std::string &responseBody) {
        Json::Value body;
        Json::CharReaderBuilder builder;
        std::string errs;
        std::istringstream iss(responseBody);
        if (Json::parseFromStream(builder, iss, &body, &errs)) {
          auto response = drogon::HttpResponse::newHttpJsonResponse(body);
          if (body.isObject() && body["status"].isString() &&
              body["status"].asString() == "error") {
            response->setStatusCode(drogon::k404NotFound);
          }
          callback(response);
        } else {
          Json::Value errorBody;
          errorBody["status"] = "error";
          errorBody["message"] = "Failed to parse device data";
          auto response = drogon::HttpResponse::newHttpJsonResponse(errorBody);
          response->setStatusCode(drogon::k500InternalServerError);
          callback(response);
        }
      },
      [callback](const std::string &error) {
        Json::Value body;
        body["status"] = "error";
        body["message"] = error;
        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
      });
}
