#pragma once

#include "TestSupport.h"
#include "common/http/HttpTransport.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace test {

/// HTTP-транспорт для клиентских тестов на базе FakeIt.
class HttpTransportMock {
 public:
  /// Задаёт ответ, который транспорт возвращает по умолчанию.
  HttpTransportMock();

  /// Передаёт мок клиенту через интерфейс транспорта.
  std::shared_ptr<iot::http::IHttpTransport> shared() const;

  /// Даёт доступ к моку для проверки вызовов через FakeIt.
  fakeit::Mock<iot::http::IHttpTransport> &mock();

  /// Возвращает все запросы, отправленные клиентом.
  const std::vector<iot::http::Request> &requests() const;

  /// Возвращает последний запрос или выбрасывает исключение, если запросов нет.
  const iot::http::Request &lastRequest() const;

  /// Позволяет задать результат отправки запроса.
  iot::http::Result &result();

  /// Позволяет задать HTTP-ответ.
  std::optional<iot::http::Response> &response();

 private:
  struct State {
    std::vector<iot::http::Request> requests;
    iot::http::Result result{iot::http::Result::Ok};
    std::optional<iot::http::Response> response{
        iot::http::Response{200, "{}", true, {}, {}}};
  };

  std::shared_ptr<State> state_;
  std::shared_ptr<fakeit::Mock<iot::http::IHttpTransport>> mock_;
};

inline HttpTransportMock::HttpTransportMock()
    : state_(std::make_shared<State>()),
      mock_(std::make_shared<fakeit::Mock<iot::http::IHttpTransport>>()) {
  const auto state = state_;
  fakeit::When(Method(*mock_, send))
      .AlwaysDo([state](const iot::http::Request &request,
                        iot::http::Completion completion) {
        state->requests.push_back(request);
        if (completion) completion(state->result, state->response);
      });
}

inline std::shared_ptr<iot::http::IHttpTransport>
HttpTransportMock::shared() const {
  return sharedMock(mock_);
}

inline fakeit::Mock<iot::http::IHttpTransport> &HttpTransportMock::mock() {
  return *mock_;
}

inline const std::vector<iot::http::Request> &
HttpTransportMock::requests() const {
  return state_->requests;
}

inline const iot::http::Request &HttpTransportMock::lastRequest() const {
  if (state_->requests.empty()) {
    throw std::logic_error("No HTTP request has been sent");
  }
  return state_->requests.back();
}

inline iot::http::Result &HttpTransportMock::result() {
  return state_->result;
}

inline std::optional<iot::http::Response> &HttpTransportMock::response() {
  return state_->response;
}

}  // namespace test
