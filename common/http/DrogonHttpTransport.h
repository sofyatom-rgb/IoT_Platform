#pragma once

#include <memory>
#include <string>

#include "common/http/HttpTransport.h"

namespace iot::http {

class DrogonHttpTransport final : public IHttpTransport {
 public:
  explicit DrogonHttpTransport(std::string baseUrl);

  void send(const Request &request, Completion completion) const override;

 private:
  std::string baseUrl_;
};

}  // namespace iot::http
