#pragma once

#include <drogon/drogon.h>

#include <functional>

#include "codec/Decoder.h"
#include "codec/Encoder.h"
#include "ingest/services/AdapterService.h"

// HTTP-слой адаптера.
// Вход от датчиков — зашифрованный конверт {checksum, ciphertext}.
// Выход к iot_core — обычный JSON (adapter раскодирует).
// Ответ датчику — снова конверт (adapter кодирует).
class IngestController {
 public:
  IngestController();

  void registerRoutes();

  void ingest(const drogon::HttpRequestPtr &request,
              std::function<void(const drogon::HttpResponsePtr &)> callback);

  void getTelemetry(const drogon::HttpRequestPtr &request,
                    std::function<void(const drogon::HttpResponsePtr &)> callback);

  void deleteTelemetry(const drogon::HttpRequestPtr &request,
                       std::function<void(const drogon::HttpResponsePtr &)> callback);

 private:
  // Разбирает конверт от датчика в обычный Json (или возвращает false + error).
  bool decodeRequest(const Json::Value &envelope,
                     Json::Value &plain,
                     std::string &error) const;

  // Оборачивает Json-ответ в конверт (если codec включён) и делает HttpResponse.
  drogon::HttpResponsePtr encodeResponse(const Json::Value &body) const;

  bool codecEnabled_;
  codec::Encoder encoder_;
  codec::Decoder decoder_;
};