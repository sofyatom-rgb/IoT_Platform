#pragma once

#include <drogon/HttpFilter.h>

// Проверяет Bearer-токен и кладёт user_id в атрибуты запроса.
class AuthFilter : public drogon::HttpFilter<AuthFilter> {
 public:
  void doFilter(const drogon::HttpRequestPtr &req,
                drogon::FilterCallback &&fcb,
                drogon::FilterChainCallback &&fccb) override;
};