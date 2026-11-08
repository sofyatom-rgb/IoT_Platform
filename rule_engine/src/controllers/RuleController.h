#pragma once

#include <drogon/drogon.h>

// HTTP-слой Rule Engine: принимает телеметрию и управляет правилами.
class RuleController {
 public:
  void registerRoutes();

  void processTelemetry(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  void createRule(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  void getRuleById(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  void getRulesForDevice(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  void updateRule(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)> callback);

  void deleteRule(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)> callback);
};