#pragma once

#include <string>

#include "common/http/HttpTransport.h"

namespace iot::http {

/// Реализация HTTP-порта на базе Drogon.
class DrogonHttpTransport final : public IHttpTransport {
 public:
  /// Создаёт транспорт для указанного базового URL.
  explicit DrogonHttpTransport(std::string baseUrl);

  /// Отправляет HTTP-запрос через Drogon.
  void send(const Request &request, Completion completion) const override;

 private:
  std::string baseUrl_;
};

}  // namespace iot::http
