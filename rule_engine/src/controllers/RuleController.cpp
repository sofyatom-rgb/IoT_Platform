#include "controllers/RuleController.h"

#include "services/RuleService.h"

#include <charconv>
#include <cmath>
#include <string>
#include <system_error>

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

// Собирает JSON-ответ с нужным HTTP-статусом
void reply(const Callback &cb, drogon::HttpStatusCode code,
           const Json::Value &body) {
  auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
  resp->setStatusCode(code);
  cb(resp);
}

Json::Value errorJson(const std::string &msg) {
  Json::Value j;
  j["status"] = "error";
  j["message"] = msg;
  return j;
}

// Ошибки БД логируем у себя, клиенту отдаём общий текст
void replyDbError(const Callback &cb, const std::string &msg) {
  LOG_ERROR << "DB error: " << msg;
  reply(cb, drogon::k500InternalServerError, errorJson("internal error"));
}

Json::Value ruleToJson(const Rule &r) {
  Json::Value j;
  j["rule_id"] = r.rule_id;
  j["device_id"] = r.device_id;
  j["operator"] = r.op;
  j["value"] = r.value;
  return j;
}

bool validOperator(const std::string &op) {
  return op == ">" || op == "<" || op == "=" || op == "!=";
}

bool parsePositiveId(const std::string &text, int &id) {
  if (text.empty()) return false;
  const auto *first = text.data();
  const auto *last = first + text.size();
  const auto [end, error] = std::from_chars(first, last, id);
  return error == std::errc{} && end == last && id > 0;
}

}  // namespace

// RuleController::RuleController(std::string notificationUrl)
//     : notificationUrl_(std::move(notificationUrl)) {}

void RuleController::registerRoutes() {
  auto makeService = []() {
    auto url = drogon::app()
                   .getCustomConfig()
                   .get("notification_service_url", "http://127.0.0.1:8082")
                   .asString();
    return std::make_shared<RuleService>(drogon::app().getDbClient(), url);
  };

  // GET /health — проверка доступности сервиса.
  drogon::app().registerHandler(
      "/health",
      [](const drogon::HttpRequestPtr &, Callback &&cb) {
        Json::Value body;
        body["status"] = "ok";
        cb(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      {drogon::Get});

  // POST /rules — создать правило
  drogon::app().registerHandler(
      "/rules",
      [makeService](const drogon::HttpRequestPtr &req, Callback &&cb) {
        auto json = req->getJsonObject();
        if (!json || !json->isMember("device_id") ||
            !json->isMember("operator") || !json->isMember("value") ||
            !(*json)["device_id"].isInt() || !(*json)["value"].isNumeric() ||
            !(*json)["operator"].isString()) {
          reply(cb, drogon::k400BadRequest,
                errorJson("device_id, operator, value are required"));
          return;
        }
        int deviceId = (*json)["device_id"].asInt();
        std::string op = (*json)["operator"].asString();
        double value = (*json)["value"].asDouble();
        if (deviceId <= 0 || !std::isfinite(value)) {
          reply(cb, drogon::k400BadRequest,
                errorJson("device_id must be positive and value must be finite"));
          return;
        }
        if (!validOperator(op)) {
          reply(cb, drogon::k400BadRequest,
                errorJson("operator must be one of: > < = !="));
          return;
        }

        makeService()->createRule(
            deviceId, op, value,
            [cb](int id) {
              Json::Value ok;
              ok["status"] = "ok";
              ok["rule_id"] = id;
              auto response = drogon::HttpResponse::newHttpJsonResponse(ok);
              response->setStatusCode(drogon::k201Created);
              response->addHeader("Location", "/rules/" + std::to_string(id));
              cb(response);
            },
            [cb](const std::string &msg) { replyDbError(cb, msg); });
      },
      {drogon::Post});

  // GET /rules?device_id=13 — правила устройства
  drogon::app().registerHandler(
      "/rules",
      [makeService](const drogon::HttpRequestPtr &req, Callback &&cb) {
        auto idStr = req->getParameter("device_id");
        if (idStr.empty()) {
          reply(cb, drogon::k400BadRequest,
                errorJson("device_id query parameter is required"));
          return;
        }
        int deviceId = 0;
        if (!parsePositiveId(idStr, deviceId)) {
          reply(cb, drogon::k400BadRequest,
                errorJson("device_id must be a positive integer"));
          return;
        }
        makeService()->getRulesForDevice(
            deviceId,
            [cb](const std::vector<Rule> &rules) {
              Json::Value arr(Json::arrayValue);
              for (const auto &r : rules) arr.append(ruleToJson(r));
              reply(cb, drogon::k200OK, arr);
            },
            [cb](const std::string &msg) { replyDbError(cb, msg); });
      },
      {drogon::Get});

  // GET /rules/{id}
  drogon::app().registerHandler(
      "/rules/{id}",
      [makeService](const drogon::HttpRequestPtr &, Callback &&cb, int id) {
        if (id <= 0) {
          reply(cb, drogon::k400BadRequest, errorJson("id must be positive"));
          return;
        }
        makeService()->getRuleById(
            id,
            [cb](const Rule &r) { reply(cb, drogon::k200OK, ruleToJson(r)); },
            [cb](const std::string &msg) {
              if (msg == "Rule not found") {
                reply(cb, drogon::k404NotFound, errorJson(msg));
              } else {
                replyDbError(cb, msg);
              }
            });
      },
      {drogon::Get});

  // PUT /rules/{id}
  drogon::app().registerHandler(
      "/rules/{id}",
      [makeService](const drogon::HttpRequestPtr &req, Callback &&cb, int id) {
        if (id <= 0) {
          reply(cb, drogon::k400BadRequest, errorJson("id must be positive"));
          return;
        }
        auto json = req->getJsonObject();
        if (!json || !json->isMember("device_id") ||
            !(*json)["device_id"].isInt() || !json->isMember("operator") ||
            !(*json)["operator"].isString() || !json->isMember("value") ||
            !(*json)["value"].isNumeric()) {
          reply(cb, drogon::k400BadRequest,
                errorJson("device_id, operator, value are required"));
          return;
        }
        const int deviceId = (*json)["device_id"].asInt();
        std::string op = (*json)["operator"].asString();
        const double value = (*json)["value"].asDouble();
        if (deviceId <= 0 || !std::isfinite(value)) {
          reply(cb, drogon::k400BadRequest,
                errorJson("device_id must be positive and value must be finite"));
          return;
        }
        if (!validOperator(op)) {
          reply(cb, drogon::k400BadRequest, errorJson("bad operator"));
          return;
        }
        makeService()->updateRule(
            id, deviceId, op, value,
            [cb]() {
              Json::Value ok;
              ok["status"] = "ok";
              reply(cb, drogon::k200OK, ok);
            },
            [cb](const std::string &msg) { replyDbError(cb, msg); });
      },
      {drogon::Put});

  // DELETE /rules/{id}
  drogon::app().registerHandler(
      "/rules/{id}",
      [makeService](const drogon::HttpRequestPtr &, Callback &&cb, int id) {
        if (id <= 0) {
          reply(cb, drogon::k400BadRequest, errorJson("id must be positive"));
          return;
        }
        makeService()->deleteRule(
            id,
            [cb]() {
              auto resp = drogon::HttpResponse::newHttpResponse();
              resp->setStatusCode(drogon::k204NoContent);
              cb(resp);
            },
            [cb](const std::string &msg) { replyDbError(cb, msg); });
      },
      {drogon::Delete});

  // POST /telemetry-events — обработать новое показание датчика.
  drogon::app().registerHandler(
      "/telemetry-events",
      [makeService](const drogon::HttpRequestPtr &req, Callback &&cb) {
        auto json = req->getJsonObject();
        if (!json || !json->isMember("device_id") ||
            !json->isMember("temperature") ||
            !(*json)["device_id"].isInt() ||
            !(*json)["temperature"].isNumeric()) {
          reply(cb, drogon::k400BadRequest,
                errorJson("device_id and temperature are required"));
          return;
        }
        int deviceId = (*json)["device_id"].asInt();
        double temperature = (*json)["temperature"].asDouble();
        if (deviceId <= 0 || !std::isfinite(temperature)) {
          reply(cb, drogon::k400BadRequest,
                errorJson("device_id must be positive and temperature must be finite"));
          return;
        }

        makeService()->checkAndNotify(
            deviceId, temperature,
            [cb](const std::string& notificationStatus) {
              Json::Value ok;
              ok["status"] = "ok";
              ok["notification_status"] = notificationStatus;
              reply(cb, drogon::k200OK, ok);
            },
            [cb](const std::string &msg) {
              LOG_ERROR << "checkAndNotify failed: " << msg;
              reply(cb, drogon::k500InternalServerError,
                    errorJson(msg));
            });
      },
      {drogon::Post});
}