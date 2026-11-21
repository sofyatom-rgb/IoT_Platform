#include "ingest/controllers/IngestController.h"
#include "ingest/clients/IoTClient.h"
#include "common/http/DrogonHttpTransport.h"
#include "common/validation/RequestValidation.h"

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
  drogon::app().registerHandler(
      "/ingest",
      [this](const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        ingest(req, std::move(cb));
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/telemetry/query",
      [this](const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        getTelemetry(req, std::move(cb));
      },
      {drogon::Post});

  drogon::app().registerHandler(
      "/telemetry",
      [this](const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        deleteTelemetry(req, std::move(cb));
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

  if (!payload["device_id"].isInt() || !payload["temperature"].isNumeric() ||
      !iot::validation::isValidTelemetryInput(
          payload["device_id"].asInt(), payload["temperature"].asDouble())) {
    Json::Value body;
    body["status"]  = "error";
    body["message"] = "Decoded payload must contain positive integer device_id and finite numeric temperature";
    auto response = encodeResponse(body);
    response->setStatusCode(drogon::k400BadRequest);
    callback(response);
    return;
  }

  AdapterService service(iotClient_);

  service.processTelemetry(
      payload["device_id"].asInt(),
      payload["temperature"].asDouble(),
      [callback, this]() {
        Json::Value body;
        body["status"]  = "ok";
        body["message"] = "Data forwarded to IoT Core";
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
