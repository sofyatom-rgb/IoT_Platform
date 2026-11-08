#pragma once

#include <memory>

#include "common/http/HttpTransport.h"
#include "telemetry/clients/IRuleEngineClient.h"

class RuleEngineClient final : public IRuleEngineClient {
 public:
  RuleEngineClient(std::string baseUrl,
                   std::shared_ptr<iot::http::IHttpTransport> transport);

  void checkTelemetry(
      int deviceId, double temperature,
      std::function<void(const std::string &)> success,
      std::function<void(const std::string &)> error) const override;

 private:
  std::string baseUrl_;
  std::shared_ptr<iot::http::IHttpTransport> transport_;
};
