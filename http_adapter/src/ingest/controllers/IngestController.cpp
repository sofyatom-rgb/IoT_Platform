#include "ingest/controllers/IngestController.h"

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
    : codecEnabled_(drogon::app().getCustomConfig().get("codec_enabled", true).asBool()),
      encoder_(drogon::app().getCustomConfig()["codec_key"].asString()),
      decoder_(drogon::app().getCustomConfig()["codec_key"].asString()) {}

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
    callback(encodeResponse(body));
    return;
  }

  Json::Value payload;
  std::string err;
  if (!decodeRequest(*env, payload, err)) {
    Json::Value body;
    body["status"]  = "error";
    body["message"] = "Decode failed: " + err;
    callback(encodeResponse(body));
    return;
  }

  if (!payload["device_id"].isInt() || !payload["temperature"].isNumeric()) {
    Json::Value body;
    body["status"]  = "error";
    body["message"] = "Decoded payload must contain integer device_id and numeric temperature";
    callback(encodeResponse(body));
    return;
  }

  const std::string iotCoreUrl =
      drogon::app().getCustomConfig()["iot_core_url"].asString();
  AdapterService service(iotCoreUrl);

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
        callback(encodeResponse(body));
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
    callback(encodeResponse(body));
    return;
  }

  Json::Value payload;
  std::string err;
  if (!decodeRequest(*env, payload, err)) {
    Json::Value body;
    body["status"]  = "error";
    body["message"] = "Decode failed: " + err;
    callback(encodeResponse(body));
    return;
  }

  if (!payload["device_id"].isInt() || payload["device_id"].asInt() <= 0) {
    Json::Value body;
    body["status"]  = "error";
    body["message"] = "Expected a positive integer device_id";
    callback(encodeResponse(body));
    return;
  }

  const std::string iotCoreUrl =
      drogon::app().getCustomConfig()["iot_core_url"].asString();
  AdapterService service(iotCoreUrl);

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
        callback(encodeResponse(body));
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
    callback(encodeResponse(body));
    return;
  }

  Json::Value payload;
  std::string err;
  if (!decodeRequest(*env, payload, err)) {
    Json::Value body;
    body["status"]  = "error";
    body["message"] = "Decode failed: " + err;
    callback(encodeResponse(body));
    return;
  }

  if (!payload["device_id"].isInt() || payload["device_id"].asInt() <= 0) {
    Json::Value body;
    body["status"]  = "error";
    body["message"] = "Expected a positive integer device_id";
    callback(encodeResponse(body));
    return;
  }

  const std::string iotCoreUrl =
      drogon::app().getCustomConfig()["iot_core_url"].asString();
  AdapterService service(iotCoreUrl);

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
        callback(encodeResponse(body));
      });
}