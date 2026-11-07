#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <variant>

namespace iot::http {

enum class Method { Get, Post, Put, Delete };
using JsonScalar = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string>;
using JsonObject = std::map<std::string, JsonScalar>;

struct Request {
  Method method{Method::Get};
  std::string path;
  JsonObject jsonBody;
};

struct Response {
  int statusCode{0};
  std::string body;
  bool jsonParsed{false};
  JsonObject jsonBody;
  std::string jsonParseError;
};

enum class Result { Ok, NetworkError };
using Completion = std::function<void(Result, std::optional<Response>)>;

// Порт HTTP-транспорта. В приложении используется DrogonHttpTransport,
// а в юнит-тестах — синхронный fake, который не открывает сетевых соединений.
class IHttpTransport {
 public:
  virtual ~IHttpTransport() = default;
  virtual void send(const Request &request, Completion completion) const = 0;
};

inline const std::string *stringField(const JsonObject &object,
                                      const std::string &name) {
  const auto it = object.find(name);
  if (it == object.end()) return nullptr;
  return std::get_if<std::string>(&it->second);
}

inline const bool *boolField(const JsonObject &object, const std::string &name) {
  const auto it = object.find(name);
  if (it == object.end()) return nullptr;
  return std::get_if<bool>(&it->second);
}

}  // namespace iot::http
