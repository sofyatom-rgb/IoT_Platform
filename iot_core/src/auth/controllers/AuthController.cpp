#include "common/db/DbClientProvider.h"
// AuthController.cpp
#include "auth/controllers/AuthController.h"
#include "auth/services/AuthService.h"

namespace {
using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

void replyError(const Callback &cb, drogon::HttpStatusCode code,
                const std::string &msg) {
  Json::Value b; b["status"] = "error"; b["message"] = msg;
  auto r = drogon::HttpResponse::newHttpJsonResponse(b);
  r->setStatusCode(code);
  cb(r);
}
}  // namespace

void AuthController::registerRoutes() {
  drogon::app().registerHandler(
      "/auth/login",
      [](const drogon::HttpRequestPtr &req, Callback &&cb) {
        auto json = req->getJsonObject();
        if (!json || !(*json)["email"].isString() ||
            !(*json)["password"].isString()) {
          replyError(cb, drogon::k400BadRequest, "email and password required");
          return;
        }
        auto service = std::make_shared<AuthService>(DbClientProvider::get());
        service->login(
            (*json)["email"].asString(), (*json)["password"].asString(),
            [cb, service](const std::string &token, int userId) {
              Json::Value ok;
              ok["token"] = token;
              ok["user_id"] = userId;
              cb(drogon::HttpResponse::newHttpJsonResponse(ok));
            },
            [cb]() {
              replyError(cb, drogon::k401Unauthorized,
                         "invalid email or password");
            },
            [cb](const std::string &e) {
              LOG_ERROR << "login DB error: " << e;
              replyError(cb, drogon::k500InternalServerError, "internal error");
            });
      },
      {drogon::Post});
}