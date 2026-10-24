#pragma once

#include <drogon/drogon.h>

#include "ingest/services/AdapterService.h"

// HTTP-слой адаптера: принимает JSON от внешних систем.
class IngestController {
 public:
  void registerRoutes();

  void ingest(const drogon::HttpRequestPtr &request,
              std::function<void(const drogon::HttpResponsePtr &)> callback);

  void getTelemetry(const drogon::HttpRequestPtr &request,
                    std::function<void(const drogon::HttpResponsePtr &)> callback);

  void deleteTelemetry(const drogon::HttpRequestPtr &request,
                       std::function<void(const drogon::HttpResponsePtr &)> callback);
};