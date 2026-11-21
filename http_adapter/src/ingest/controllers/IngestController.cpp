#include "ingest/controllers/IngestController.h"
#include "ingest/clients/IoTClient.h"
#include "common/http/DrogonHttpTransport.h"
#include "common/validation/RequestValidation.h"

#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <stdexcept>

#include <sstream>
#include <string>
#include <utility>

namespace {

std::string toJsonString(const Json::Value &v) {
  Json::StreamWriterBuilder wb;
  wb["indentation"] = "";
  wb["emitUTF8"]    = true;
  return Json::writeString(wb, v);
}

bool parseJsonString(const std::string &text, Json::Value &out, std::string &err) {
  Json::CharReaderBuilder rb;
  std::istringstream iss(text);
  return Json::parseFromStream(rb, iss, &out, &err);
}

Json::Value makeJson(
    std::initializer_list<std::pair<const char *, Json::Value>> fields) {
  Json::Value body(Json::objectValue);
  for (const auto &field : fields) {
    body[field.first] = field.second;
  }
  return body;
}

}  // namespace

IngestController::IngestController()
    : IngestController(
          drogon::app().getCustomConfig().get("codec_enabled", true).asBool(),
          drogon::app().getCustomConfig()["codec_key"].asString(),
          std::make_shared<IoTClient>(
              std::make_shared<iot::http::DrogonHttpTransport>(
                  drogon::app().getCustomConfig()["iot_core_url"].asString()))) {}

IngestController::IngestController(
    bool codecEnabled, std::string codecKey,
    std::shared_ptr<IIoTClient> iotClient)
    : codecEnabled_(codecEnabled),
      encoder_(codecKey),
      decoder_(std::move(codecKey)),
      iotClient_(std::move(iotClient)) {
  if (!iotClient_) {
    throw std::invalid_argument("IngestController requires an IoT client");
  }
}

void IngestController::registerRoutes() {
  // GET /health -> проверить доступность адаптера.
  drogon::app().registerHandler(
      "/health",
      [](const drogon::HttpRequestPtr &,
         std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        Json::Value body;
        body["status"] = "ok";
        cb(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      {drogon::Get});

  // device_id в URI, тело содержит только temperature.
  drogon::app().registerHandler(
      "/devices/{id}/telemetry",
      [this](const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&cb,
             int deviceId) {
        req->setParameter("device_id", std::to_string(deviceId));
        ingest(req, std::move(cb));
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/devices/{id}/telemetry",
      [this](const drogon::HttpRequestPtr &,
             std::function<void(const drogon::HttpResponsePtr &)> &&cb,
             int deviceId) {
        iotClient_->getTelemetry(
            deviceId,
            [cb](const std::string &responseBody) {
              auto response = drogon::HttpResponse::newHttpResponse();
              response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
              response->setBody(responseBody);
              cb(response);
            },
            [cb](const std::string &error) {
              Json::Value body;
              body["status"] = "error";
              body["message"] = error;
              auto response = drogon::HttpResponse::newHttpJsonResponse(body);
              response->setStatusCode(drogon::k502BadGateway);
              cb(response);
            });
      },
      {drogon::Get});

  drogon::app().registerHandler(
      "/devices/{id}/telemetry",
      [this](const drogon::HttpRequestPtr &,
             std::function<void(const drogon::HttpResponsePtr &)> &&cb,
             int deviceId) {
        iotClient_->deleteTelemetry(
            deviceId,
            [cb]() {
              auto response = drogon::HttpResponse::newHttpResponse();
              response->setStatusCode(drogon::k204NoContent);
              cb(response);
            },
            [cb](const std::string &error) {
              Json::Value body;
              body["status"] = "error";
              body["message"] = error;
              auto response = drogon::HttpResponse::newHttpJsonResponse(body);
              response->setStatusCode(drogon::k502BadGateway);
              cb(response);
            });
      },
      {drogon::Delete});
}

bool IngestController::decodeRequest(const Json::Value &envelope,
                                     Json::Value &plain,
                                     std::string &error) const {
  if (!codecEnabled_) {
    plain = envelope;
    return true;
  }
  std::string plaintext;
  if (!decoder_.decode(toJsonString(envelope), plaintext, error)) {
    return false;
  }
  return parseJsonString(plaintext, plain, error);
}

drogon::HttpResponsePtr IngestController::encodeResponse(const Json::Value &body) const {
  if (!codecEnabled_) {
    return drogon::HttpResponse::newHttpJsonResponse(body);
  }
  const std::string envelope = encoder_.encode(toJsonString(body));
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  resp->setBody(envelope);
  return resp;
}

void IngestController::ingest(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto reply = [](const Json::Value &body,
                        drogon::HttpStatusCode status) {
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    return response;
  };

  const auto json = request->getJsonObject();
  if (!json) {
    callback(reply(makeJson({{"status", "error"},
                             {"message", "Invalid JSON in Adapter"}}),
                   drogon::k400BadRequest));
    return;
  }

  // device_id приходит в URI, в теле только temperature.
  const std::string idParam = request->getParameter("device_id");
  int deviceId = 0;
  try {
    deviceId = std::stoi(idParam);
  } catch (...) {
    deviceId = 0;
  }
  if (!iot::validation::isPositiveId(deviceId) ||
      !(*json)["temperature"].isNumeric() ||
      !iot::validation::isValidTelemetryInput(deviceId,
                                              (*json)["temperature"].asDouble())) {
    callback(reply(makeJson(
                       {{"status", "error"},
                        {"message", "Expected a positive device ID in the URI and finite numeric temperature"}}),
                   drogon::k400BadRequest));
    return;
  }

  AdapterService service(iotClient_);
  service.processTelemetry(
      deviceId, (*json)["temperature"].asDouble(),
      [callback, reply](const std::string &notificationStatus) {
        callback(reply(makeJson({{"status", "ok"},
                                 {"message", "Data forwarded to IoT Core"},
                                 {"notification_status", notificationStatus}}),
                       drogon::k201Created));
      },
      [callback, reply](const std::string &error) {
        callback(reply(makeJson({{"status", "error"}, {"message", error}}),
                       drogon::k502BadGateway));
      });
}

void IngestController::getTelemetry(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto env = request->getJsonObject();
  if (!env) {
    Json::Value body;
    body["status"]  = "error";
    body["message"] = "Invalid JSON in Adapter";
    auto response = encodeResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  Json::Value payload;
  std::string err;
  if (!decodeRequest(*env, payload, err)) {
    Json::Value body;
    body["status"]  = "error";
    body["message"] = "Decode failed: " + err;
    auto response = encodeResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  if (!payload["device_id"].isInt() ||
      !iot::validation::isPositiveId(payload["device_id"].asInt())) {
    Json::Value body;
    body["status"]  = "error";
    body["message"] = "Expected a positive integer device_id";
    auto response = encodeResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  AdapterService service(iotClient_);

  service.getTelemetry(
      payload["device_id"].asInt(),
      [callback, this](const std::string &responseBody) {
        Json::Value body;
        body["status"] = "ok";

        Json::Value dataArray;
        std::string perr;
        if (parseJsonString(responseBody, dataArray, perr)) {
          body["data"] = dataArray;
        } else {
          body["data_raw"] = responseBody;
        }
        callback(encodeResponse(body));
      },
      [callback, this](const std::string &error) {
        Json::Value body;
        body["status"]  = "error";
        body["message"] = error;
        auto response = encodeResponse(body);
        response->setStatusCode(drogon::k502BadGateway);
        callback(response);
      });
}

void IngestController::deleteTelemetry(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
  const auto env = request->getJsonObject();
  if (!env) {
    Json::Value body;
    body["status"]  = "error";
    body["message"] = "Invalid JSON in Adapter";
    auto response = encodeResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  Json::Value payload;
  std::string err;
  if (!decodeRequest(*env, payload, err)) {
    Json::Value body;
    body["status"]  = "error";
    body["message"] = "Decode failed: " + err;
    auto response = encodeResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  if (!payload["device_id"].isInt() ||
      !iot::validation::isPositiveId(payload["device_id"].asInt())) {
    Json::Value body;
    body["status"]  = "error";
    body["message"] = "Expected a positive integer device_id";
    auto response = encodeResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  AdapterService service(iotClient_);

  service.deleteTelemetry(
      payload["device_id"].asInt(),
      [callback, this]() {
        Json::Value body;
        body["status"]  = "ok";
        body["message"] = "Telemetry deleted via adapter";
        callback(encodeResponse(body));
      },
      [callback, this](const std::string &error) {
        Json::Value body;
        body["status"]  = "error";
        body["message"] = error;
        auto response = encodeResponse(body);
        response->setStatusCode(drogon::k502BadGateway);
        callback(response);
      });
}
