#pragma once

#include "TestMocks.h"

#include "common/http/HttpTransport.h"

#include <memory>
#include <optional>
#include <vector>

namespace test {

// FakeIt-транспорт: запоминает запросы и отдаёт заранее заданный ответ.
class HttpTransportMock {
 public:
  HttpTransportMock();

  // Интерфейс для клиентов; мок живёт столько же, сколько этот объект.
  std::shared_ptr<iot::http::IHttpTransport> shared() const;

  // Последний отправленный запрос.
  const iot::http::Request &lastRequest() const;

  mutable std::vector<iot::http::Request> requests;
  iot::http::Result result{iot::http::Result::Ok};
  std::optional<iot::http::Response> response{
      iot::http::Response{200, "{}", true, {}, {}}};

 private:
  std::shared_ptr<fakeit::Mock<iot::http::IHttpTransport>> mock_ =
      std::make_shared<fakeit::Mock<iot::http::IHttpTransport>>();
};

inline HttpTransportMock::HttpTransportMock() {
  fakeit::When(Method(*mock_, send))
      .AlwaysDo([this](const iot::http::Request &request,
                       iot::http::Completion completion) {
        requests.push_back(request);
        if (completion) completion(result, response);
      });
}

inline std::shared_ptr<iot::http::IHttpTransport> HttpTransportMock::shared()
    const {
  return sharedMock(mock_);
}

inline const iot::http::Request &HttpTransportMock::lastRequest() const {
  return requests.back();
}

}  // namespace test
