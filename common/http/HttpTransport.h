#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <variant>

namespace iot::http {

/// HTTP-метод, используемый внутренними клиентами.
enum class Method { Get, Post, Put, Delete };

/// Типы JSON-значений, которые отправляют клиенты.
using JsonScalar = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string>;

/// Объект с простыми JSON-полями.
using JsonObject = std::map<std::string, JsonScalar>;

/// Данные исходящего HTTP-запроса.
struct Request {
  Method method{Method::Get};
  std::string path;
  JsonObject jsonBody;
};

/// Данные HTTP-ответа и результат разбора JSON-тела.
struct Response {
  int statusCode{0};
  std::string body;
  bool jsonParsed{false};
  JsonObject jsonBody;
  std::string jsonParseError;
};

/// Итог отправки запроса на транспортном уровне.
enum class Result { Ok, NetworkError };

/// Callback с результатом отправки и, если есть, HTTP-ответом.
using Completion = std::function<void(Result, std::optional<Response>)>;

/// HTTP-порт, отделяющий клиентов приложения от Drogon и реальной сети.
class IHttpTransport {
 public:
  /// Позволяет удалять реализацию через указатель на интерфейс.
  virtual ~IHttpTransport() = default;

  /// Отправляет запрос и передаёт результат в callback.
  virtual void send(const Request &request, Completion completion) const = 0;
};

/// Возвращает строковое поле JSON-объекта или nullptr, если его нет.
inline const std::string *stringField(const JsonObject &object,
                                      const std::string &name) {
  const auto it = object.find(name);
  if (it == object.end()) return nullptr;
  return std::get_if<std::string>(&it->second);
}

/// Возвращает логическое поле JSON-объекта или nullptr, если его нет.
inline const bool *boolField(const JsonObject &object, const std::string &name) {
  const auto it = object.find(name);
  if (it == object.end()) return nullptr;
  return std::get_if<bool>(&it->second);
}

}  // namespace iot::http
