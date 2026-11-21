#include "auth/filters/AuthFilter.h"

#include <optional>
#include <string>

#include "auth/services/TokenService.h"

void AuthFilter::doFilter(const drogon::HttpRequestPtr &req,
                          drogon::FilterCallback &&fcb,
                          drogon::FilterChainCallback &&fccb) {
  const std::string h = req->getHeader("Authorization");
  std::optional<int> userId;
  if (h.rfind("Bearer ", 0) == 0) {
    userId = TokenService::verify(h.substr(7));
  }
  if (!userId) {
    Json::Value b;
    b["status"] = "error";
    b["message"] = "unauthorized";
    auto r = drogon::HttpResponse::newHttpJsonResponse(b);
    r->setStatusCode(drogon::k401Unauthorized);
    fcb(r);
    return;
  }
  req->attributes()->insert("user_id", *userId);
  fccb();
}